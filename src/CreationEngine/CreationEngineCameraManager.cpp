//
// Created by sergp on 6/23/2024.
//

#include "CreationEngineCameraManager.h"
#include "CreationEngineConstants.h"
#include "CreationEngineRendererModule.h"
#include "CreationEngineSingletonManager.h"
#include "FirstPersonPointMapping.h"
#include "RE/P/PlayerCamera.h"
#include "RE/S/ScaleformGFxMovie.h"
#include <CreationEngine/memory/offsets.h>
#include <CreationEngine/models/GameFlow.h>
#include <CreationEngine/models/ModSettingsStore.h>
#include <REL/Relocation.h>
#include <atomic>
#include <cstdlib>
#include <glm/gtx/vector_angle.hpp>
#include <mods/VR.hpp>

#include "ModSettings.h"

namespace {
    const  glm::mat4 permutation_pre = {
        1, 0, 0, 0,
        0, 0, 1, 0,
        0, -1, 0, 0,
        0, 0,  0, 1
    };
    const glm::mat4 permutation_post = glm::transpose(permutation_pre);

    glm::mat4 to_havok_space(const glm::mat4& mat) {
        return permutation_pre * mat * permutation_post;
    }

    glm::mat4 from_havok_space(const glm::mat4& mat) {
        return permutation_post * mat * permutation_pre;
    }

    std::atomic<float> applied_head_yaw{ 0.0f };
    bool               head_only_pitch_active{ };
}


void onSetNimFrustumDetour(RE::NiCamera *camera, RE::NiFrustum *frustum) {
    CreationEngineCameraManager::Get()->onSetNimFrustum(camera, frustum);
}

void onCalcNimFrustumDetour(RE::NiCamera *camera, float fov, float aspectRatio, float nearDist, float farDist,
                            char lodAdjust) {
    CreationEngineCameraManager::Get()->onCalcNiFrustum(camera, fov, aspectRatio, nearDist, farDist, lodAdjust);
}

void onScaleformSetViewPortDetour(uintptr_t *thisMovie, Scaleform::Gfx::Viewport *viewport) {
    CreationEngineCameraManager::Get()->onScaleformSetViewPort(thisMovie, viewport);
}

/*

void onScaleformMovieSetProjectionMatrix3DDetour(uintptr_t* thisMovie, Matrix4x4f& matrix)
{
    CreationEngineCameraManager::Get()->onScaleformMovieSetProjectionMatrix3D(thisMovie, matrix);
}
*/

bool isValidCamera(RE::NiCamera *pCamera) {
    static auto sceneGraphRoot = CreationEngineSingletonManager::GetSceneGraphRoot();
    return pCamera == sceneGraphRoot->worldCamera || pCamera == sceneGraphRoot->starfieldScene.pStarFieldCamera ||
           sceneGraphRoot->starfieldScene.pGalaxyCamera == pCamera;
}

bool CreationEngineCameraManager::InstallHooks()
{
    const auto lookPitchSlot = GameStore::MemoryOffsets::PlayerCharacter::ApplyLookPitchSlot();
    m_setLookPitch           = reinterpret_cast<SetAngle>(GameStore::MemoryOffsets::Actor::SetLookPitch());
    m_setYaw                 = reinterpret_cast<SetAngle>(GameStore::MemoryOffsets::Actor::SetYaw());
    if (!lookPitchSlot || !m_setLookPitch || !m_setYaw) {
        spdlog::error("[LOOK] Native look bindings were not found");
        return false;
    }

    REL::Relocation<uintptr_t> onNiAVObjectUpdateWorldAddr{GameStore::MemoryOffsets::BSFadeNode::vtable_UpdateWorld()};
    m_onNiAVObjectUpdateWorldHook = std::make_unique<FunctionHook>(onNiAVObjectUpdateWorldAddr.address(),
                                                                   reinterpret_cast<uintptr_t>(&onNiAVObjectUpdateWorld));
    m_onNiAVObjectUpdateWorldHook->create();

    REL::Relocation<uintptr_t> onGetCameraRotationAddr{ GameStore::MemoryOffsets::FirstPersonState::GetRotationQuatV() };
    m_onGetCameraRotationHook = std::make_unique<FunctionHook>(onGetCameraRotationAddr.address(), reinterpret_cast<uintptr_t>(&onFPSGetCameraRotation));
    m_onGetCameraRotationHook->create();


    REL::Relocation<uintptr_t> setNimFrustumVFuncAddr{GameStore::MemoryOffsets::NiCamera::SetFrustumVfunc()};
    m_onSetNimFrustumHook = std::make_unique<FunctionHook>(setNimFrustumVFuncAddr.address(),
                                                           reinterpret_cast<uintptr_t>(&onSetNimFrustumDetour));
    m_onSetNimFrustumHook->create();

    REL::Relocation<uintptr_t> calcNimFrustumVFuncAddr{GameStore::MemoryOffsets::NiCamera::CalcFrustumVfunc()};
    m_onCalcNimFrustumHook = std::make_unique<FunctionHook>(calcNimFrustumVFuncAddr.address(),
                                                            reinterpret_cast<uintptr_t>(&onCalcNimFrustumDetour));
    m_onCalcNimFrustumHook->create();

    REL::Relocation<uintptr_t> scaleformSetViewPortAddr{GameStore::MemoryOffsets::Scaleform::Movie::SetViewportVFunc()};
    m_onScaleformSetViewPortHook = std::make_unique<FunctionHook>(scaleformSetViewPortAddr.address(),
                                                                  reinterpret_cast<uintptr_t>(&onScaleformSetViewPortDetour));
    m_onScaleformSetViewPortHook->create();

    FirstPersonPointMapping::Install();

    // PointerHook publishes the detour in its constructor; make the original available first.
    m_applyLookPitch = reinterpret_cast<SetAngle>(*lookPitchSlot);
    try {
        m_onApplyLookPitchHook = std::make_unique<PointerHook>(lookPitchSlot, reinterpret_cast<void*>(&onApplyLookPitch));
    }
    catch (const std::exception& error) {
        spdlog::error("[LOOK] Cannot hook the native look update: {}", error.what());
        return false;
    }
    spdlog::info("[LOOK] Native controls installed: slot={:x}, pitch={:x}, yaw={:x}", reinterpret_cast<uintptr_t>(lookPitchSlot), reinterpret_cast<uintptr_t>(m_setLookPitch),
                 reinterpret_cast<uintptr_t>(m_setYaw));
    return true;
}

RE::NiAVObject *getCameraRootNode() {
    auto playerCamera = CreationEngineSingletonManager::GetPlayerCameraSingleton();

    if (!playerCamera || !playerCamera->IsInFirstPerson()) {
        return nullptr;
    }
    RE::NiAVObject *a_camera = playerCamera->pFirstPersonModeState->cameraRoot;
    while (a_camera) {
        if (a_camera->name == "Root") {
            return a_camera;
        }
        a_camera = a_camera->parent;
    }
    return nullptr;
}

void CreationEngineCameraManager::onApplyLookPitch(RE::PlayerCharacter* actor, float pitchDelta)
{
    static auto instance = Get();
    static auto original = instance->m_applyLookPitch;
    static auto vr       = VR::get();
    if (actor != CreationEngineSingletonManager::GetPlayerRef()) {
        return original(actor, pitchDelta);
    }
    if (!vr->is_hmd_active() || !GameFlow::isInFirstPerson() || GameFlow::isShowingMenu() || GameFlow::isImmovable() || GameFlow::isControlledByAI() || ModConstants::cameraShake) {
        head_only_pitch_active = false;
        return original(actor, pitchDelta);
    }

    const auto applied = applied_head_yaw.load(std::memory_order_relaxed);
    // The movement update calls this even with zero look input, before updating the scene transforms.
    if (GameFlow::gStore.internalSettings.pawnControl) {
        const auto head    = to_havok_space(vr->get_rotation(0));
        const auto heading = std::atan2(head[0].y, head[0].x);
        const auto delta   = std::remainder(heading - applied, 2.0f * glm::pi<float>());
        if (delta != 0.0f) {
            const auto before = actor->data.angle.z;
            instance->m_setYaw(actor, before - delta);
            const auto accepted = std::remainder(before - actor->data.angle.z, 2.0f * glm::pi<float>());
            applied_head_yaw.store(std::remainder(applied + accepted, 2.0f * glm::pi<float>()), std::memory_order_relaxed);
        }
    }

    if (!head_only_pitch_active) {
        instance->m_setLookPitch(actor, 0.0f);
        head_only_pitch_active = true;
    }
}

RE::NiQuaternion composeViewRotation(const RE::NiQuaternion& native, const RE::NiQuaternion& head, float appliedYaw)
{
    RE::NiMatrix3 rotation;
    native.ToMatrix(rotation);
    float pitch, roll, yaw;
    rotation.ToEulerAnglesXYZ(pitch, roll, yaw);
    rotation.FromEulerAnglesXYZ(0.0f, roll, yaw - appliedYaw);
    return RE::NiQuaternion(rotation) * head;
}

RE::NiPoint3 getCameraPosition(RE::NiAVObject* camera, RE::NiAVObject* root)
{
    // The camera's world transform may still be from the previous frame when the weapon rig updates.
    RE::NiPoint3 position{ };
    for (auto node = camera; node != root; node = node->parent) {
        position = node->local.translate + RE::NiQuaternion(node->local.rotate) * (position * node->local.scale);
    }
    return root->world.translate + RE::NiQuaternion(root->world.rotate) * (position * root->world.scale);
}

glm::mat4 getFirstPersonEyeTransform(glm::mat4 head, const glm::mat4& standingOrigin, const glm::mat4& eye)
{
    head[3].x -= standingOrigin[3].x;
    head[3].y -= standingOrigin[3].y;
    head[3].z -= standingOrigin[3].z;
    return to_havok_space(glm::inverse(glm::extractMatrixRotation(head)) * head * eye);
}

bool isCameraAttachment(RE::NiAVObject* object, RE::NiAVObject* camera, RE::NiAVObject* root)
{
    for (auto branch = camera; branch && branch->parent != root; branch = branch->parent) {
        if (object->parent == branch->parent) {
            return object != branch;
        }
    }
    return false;
}

void UpdateAttachment(RE::NiAVObject* object, RE::NiAVObject* root, RE::NiAVObject* camera)
{
    if (GameFlow::isImmovable() || GameFlow::isControlledByAI() || ModConstants::cameraShake) {
        return;
    }
    static auto vr              = VR::get();
    const auto  native_rotation = RE::NiQuaternion(root->world.rotate).Normalize();
    const auto  head_transform  = vr->get_transform(0);
    const auto  head            = glm::normalize(glm::quat_cast(to_havok_space(head_transform)));
    const auto  view_rotation   = composeViewRotation(native_rotation, RE::NiQuaternion(head.w, head.x, head.y, head.z), applied_head_yaw.load(std::memory_order_relaxed));
    const auto  correction      = (view_rotation * native_rotation.InvertVector()).Normalize();
    const auto  origin          = getCameraPosition(camera, root);
    const auto  rotation        = correction * RE::NiQuaternion(object->world.rotate).Normalize();
    rotation.ToMatrix(object->world.rotate);
    object->world.translate = origin + correction * (object->world.translate - origin);

    if (GameFlow::isAimingDownSights() && object->parent == root && object->name == "COM") {
        const auto aiming_eye = ModConstants::dominantEye == 0 ? VRRuntime::Eye::RIGHT : VRRuntime::Eye::LEFT;
        const auto eye        = vr->get_current_eye_transform(vr->get_current_render_eye() != aiming_eye);
        const auto offset     = getFirstPersonEyeTransform(head_transform, vr->get_transform_offset(), eye)[3];
        object->world.translate = object->world.translate + view_rotation * RE::NiPoint3{ offset.x, offset.y, offset.z };
    }
}

void CreationEngineCameraManager::onNiAVObjectUpdateWorld(RE::NiAVObject* obj, RE::NiUpdateData* a_data)
{
    static auto instance      = CreationEngineCameraManager::Get();
    using func_t              = decltype(onNiAVObjectUpdateWorld);
    static auto original_func = instance->m_onNiAVObjectUpdateWorldHook->get_original<func_t>();
    static auto vr            = VR::get();
    original_func(obj, a_data);
    if (vr->is_hmd_active() && !GameFlow::isShowingMenu()) {
        auto camera_root = getCameraRootNode();
        if (obj->parent && camera_root) {
            auto       camera            = CreationEngineSingletonManager::GetPlayerCameraSingleton()->pFirstPersonModeState->cameraRoot;
            const bool camera_attachment = isCameraAttachment(obj, camera, camera_root);
            if (obj->parent == camera_root || camera_attachment) {
                if (camera_attachment || !std::strstr(obj->name.c_str(), "Camera")) {
                    UpdateAttachment(obj, camera_root, camera);
                }
            }
        }
    }
}

void CreationEngineCameraManager::onScaleformSetViewPort(uintptr_t *thisMovie, Scaleform::Gfx::Viewport *viewport) {
    using func_t = decltype(onScaleformSetViewPortDetour);
    static auto original_func = m_onScaleformSetViewPortHook->get_original<func_t>();
    onScaleformSetViewPortInternal(thisMovie, viewport);
    original_func(thisMovie, viewport);
}

void
CreationEngineCameraManager::onScaleformSetViewPortInternal(uintptr_t *thisMovie, Scaleform::Gfx::Viewport *viewport) {

    static auto vr = VR::get();
    auto cc = reinterpret_cast<RE::Scaleform::GFx::MovieImpl *>(thisMovie);
    if (!cc || !cc->GetMovieDef()) return;
    auto file_url = cc->GetMovieDef()->GetFileURL();
    GameFlow::renderMenu(file_url);

    auto backbuffer_size = vr->get_backbuffer_size();
    auto viewport_buffer_width = viewport->bufferWidth;
    auto viewport_buffer_height = viewport->bufferHeight;

    int offset_left = 0;
    int offset_top = 0;

    auto settings = GameFlow::getMenuSettings(file_url);

    if (ModSettings::showFlatScreenDisplay() || !vr->is_hmd_active()) {
        return;
    }

    // Isolation test for the recurring Scaleform render crashes: skip only the viewport rect WRITE below
    // (the part suspected of racing with the game's own use of this struct). Menu-state tracking above
    // (renderMenu/getMenuSettings, needed for isShowingMenu()/head-tracking/quad-display) always runs.
    if (!GameFlow::gStore.internalSettings.scaleformViewportHook) {
        return;
    }

    auto width_multiplier = settings.hud_scale;
    auto height_multiplier = settings.hud_scale;

    // Keep the dominant eye fixed; offset the other eye for stereo depth.
    auto current_eye = vr->get_current_render_eye();
    if (ModConstants::dominantEye == 1) {
        offset_left = (current_eye == VRRuntime::Eye::RIGHT) ? -settings.perspective : 0;
    } else {
        offset_left = (current_eye == VRRuntime::Eye::LEFT) ? settings.perspective : 0;
    }

    auto visible_width = std::min((int) ((float) backbuffer_size[0] * width_multiplier), viewport_buffer_width);
    auto visible_height = std::min((int) ((float) backbuffer_size[1] * height_multiplier), viewport_buffer_height);
    viewport->width = visible_width;
    viewport->height = visible_height;
    viewport->left = (int) (viewport_buffer_width - visible_width) / 2 + offset_left;
    viewport->top = (int) (viewport_buffer_height - visible_height) / 2 + offset_top;
}

void CreationEngineCameraManager::onSetNimFrustum(RE::NiCamera *pCamera, RE::NiFrustum *pFrustum) {
    using func_t = decltype(onSetNimFrustumDetour);
    static auto original_func = m_onSetNimFrustumHook->get_original<func_t>();
    onSetNiFrustumInternal(pCamera, pFrustum);
    original_func(pCamera, pFrustum);
}

void aiming_adjustments(Vector4f &frustum, float fov_adjustment) {
    frustum[0] = std::tanf(std::atanf(frustum[0]) - fov_adjustment);
    frustum[1] = std::tanf(std::atanf(frustum[1]) + fov_adjustment);
    frustum[2] = std::tanf(std::atanf(frustum[2]) + fov_adjustment);
    frustum[3] = std::tanf(std::atanf(frustum[3]) - fov_adjustment);
}

void CreationEngineCameraManager::onSetNiFrustumInternal(RE::NiCamera *pCamera, RE::NiFrustum *pFrustum) {
    if (!isValidCamera(pCamera)) {
        return;
    }
    static auto vr = VR::get();
    if (!vr->is_hmd_active()) {
        return;
    }
    auto eye = vr->get_current_render_eye() == VRRuntime::Eye::LEFT ? 0 : 1;
    auto runtime = vr->get_runtime();
    Vector4f frustum = runtime->frustums[eye];
    aiming_adjustments(frustum, get_fov_adjustment());
    pFrustum->left = frustum[0];
    pFrustum->right = frustum[1];
    pFrustum->top = frustum[2];
    pFrustum->bottom = frustum[3];
}

void CreationEngineCameraManager::onCalcNiFrustum(RE::NiCamera *pCamera, float fov, float aspectRatio, float nearz,
                                                  float farz, char lodAdjust) {
    using func_t = decltype(onCalcNimFrustumDetour);
    static auto original_func = m_onCalcNimFrustumHook->get_original<func_t>();
    auto playerCamera = CreationEngineSingletonManager::GetPlayerCameraSingleton();

    //    spdlog::info("Setting frustum for camera [{}] {} camera[{} {}] setFov[{}]", fmt::ptr(pCamera), pCamera->name, playerCamera->fov, playerCamera->notViewFov, fov);
    static auto vr = VR::get();

    if (vr->is_hmd_active() && CreationEngineSingletonManager::GetSceneGraphRoot()->worldCamera == pCamera) {
        //        fov = fov + Constants::lodAdjustFov;
        m_fov_adjust = fov - playerCamera->fov;
        vr->m_nearz = nearz;
        vr->m_farz = farz;
    }
    original_func(pCamera, fov, aspectRatio, nearz, farz, lodAdjust);
}

float CreationEngineCameraManager::get_fov_adjustment() const {
    if (GameFlow::gStore.internalSettings.preventZoom) {
        return 0.0f;
    }
    return tanf((m_fov_adjust * ModConstants::DEG_TO_RAD) / 2.0f);
}

float CreationEngineCameraManager::get_head_tracking_multiplier() const {
    static auto playerCamera = CreationEngineSingletonManager::GetPlayerCameraSingleton();
    auto multiplier = (playerCamera->fov + m_fov_adjust) / playerCamera->fov;
    return multiplier * ModConstants::headTrackingMultiplier;
}


void CreationEngineCameraManager::UpdateWorldCamera() {
    static auto vr = VR::get();
    auto worldCamera = CreationEngineSingletonManager::GetSceneGraphRoot()->worldCamera;

    if (!worldCamera) {
        return;
    }

    static auto originalRotation = worldCamera->local.rotate;
    static auto originalPosition = worldCamera->local.translate;

    if (!vr->is_hmd_active() || ModConstants::cameraShake || GameFlow::isShowingMenu()) {
        worldCamera->local.rotate = originalRotation;
        worldCamera->local.translate = originalPosition;
        return;
    }

    if(!GameFlow::isImmovable() && !GameFlow::isControlledByAI() && GameFlow::isInFirstPerson()) {
        auto hmd_transform             = getFirstPersonEyeTransform(vr->get_transform(0), vr->get_transform_offset(), vr->get_current_eye_transform());
        worldCamera->local.rotate = originalRotation * *(RE::NiMatrix3*) & hmd_transform;
        worldCamera->local.translate.x = hmd_transform[3][0];
        worldCamera->local.translate.y = hmd_transform[3][1];
        worldCamera->local.translate.z = hmd_transform[3][2];
    } else {
        auto head_rotation = vr->get_transform(0);
        auto standing_origin = vr->get_transform_offset();
        head_rotation[3].x -= standing_origin[3].x;
        head_rotation[3].y -= standing_origin[3].y;
        head_rotation[3].z -= standing_origin[3].z;
        auto eye = vr->get_current_eye_transform();
        head_rotation = head_rotation * eye;
        head_rotation = to_havok_space(head_rotation);
        worldCamera->local.rotate = originalRotation * *(RE::NiMatrix3 *) &head_rotation;
        worldCamera->local.translate.x = head_rotation[3][0];
        worldCamera->local.translate.y = head_rotation[3][1];
        worldCamera->local.translate.z = head_rotation[3][2];
    }
}


void CreationEngineCameraManager::onFPSGetCameraRotation(RE::FirstPersonState *fps, RE::NiQuaternion *quat_out) {
    static auto instance = CreationEngineCameraManager::Get();
    static auto original_func = instance->m_onGetCameraRotationHook->get_original<decltype(onFPSGetCameraRotation)>();
    original_func(fps, quat_out);
    static auto vr = VR::get();
    if (!vr->is_hmd_active() || ModConstants::cameraShake || GameFlow::isShowingMenu()) {
        return;
    }
    if (!GameFlow::isImmovable() && !GameFlow::isControlledByAI()) {
        auto current_hmd_rotation = vr->get_rotation(0);
        auto rotation_quat = glm::normalize(glm::quat_cast(to_havok_space(current_hmd_rotation)));
        auto ni_hmd_rotation = RE::NiQuaternion(rotation_quat.w, rotation_quat.x, rotation_quat.y, rotation_quat.z);
        *quat_out = composeViewRotation(*quat_out, ni_hmd_rotation, applied_head_yaw.load(std::memory_order_relaxed));
    }
}
