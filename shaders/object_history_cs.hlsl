// Runs right after each GatherGeometryTransforms batch and gives the blocks it moved their transform from two frames back as the previous
// transform, the age of the per-eye camera history.
// Assumes no block occurs twice in one batch; the game's update lists come from bit masks.

struct Transform
{
    float4 rotation;
    float4 position_scale;
};

struct Block
{
    uint4     head; // y, z: current and previous bone palette, base in the low 24 bits
    Transform current;
    Transform previous;
    uint4     tail;
};

struct Update
{
    uint block; // bit 31: reset, previous = current
    uint transform;
    uint parent;
    uint packed;
};

cbuffer Batch : register(b0)
{
    uint count;
};

cbuffer Frame : register(b1)
{
    uint frame;
    uint base; // slot of this batch's first update in the frame
    uint block_capacity;
    uint slot_capacity;
};

RWStructuredBuffer<Block> blocks  : register(u0, space4);
StructuredBuffer<Update>  updates : register(t0, space5);

RWStructuredBuffer<uint2>     stamps : register(u0, space6); // per block: frame of its last gather, slot + 1 (0: none)
RWStructuredBuffer<Transform> slots  : register(u1, space6); // per update of the last two frames: the native previous transform

static const uint kReset = 0x80000000;

bool same(Transform a, Transform b)
{
    return all(asuint(a.rotation) == asuint(b.rotation)) && all(asuint(a.position_scale) == asuint(b.position_scale));
}

[numthreads(64, 1, 1)]
void CSMain(uint id : SV_DispatchThreadID)
{
    if (id >= count) {
        return;
    }
    const Update update = updates[id];
    const uint   index  = update.block & ~kReset;
    uint         size, stride;
    blocks.GetDimensions(size, stride);
    if (index >= min(size, block_capacity)) {
        return;
    }
    const uint2 last  = stamps[index];
    const Block block = blocks[index];
    const uint  slot  = base + id;
    if (slot < slot_capacity) {
        slots[(frame & 1) * slot_capacity + slot] = block.previous;
        stamps[index]                             = uint2(frame, slot + 1);
    } else {
        stamps[index] = uint2(frame, 0);
    }
    const bool gathered_last_frame = last.x == frame - 1 && last.y != 0;
    const bool moved               = !same(block.current, block.previous);
    const bool shared_palette      = (block.head.y & 0xFFFFFF) == (block.head.z & 0xFFFFFF); // Separate palettes leave skinning one frame newer than the retimed transform.
    // The moved check limits writes to blocks the engine gathers again next frame; that gather overwrites our value, so no restore is needed.
    // Removing it can leave a substituted previous transform in a block the engine no longer gathers.
    if ((update.block & kReset) == 0 && gathered_last_frame && moved && shared_palette) {
        blocks[index].previous = slots[((frame - 1) & 1) * slot_capacity + last.y - 1];
    }
}
