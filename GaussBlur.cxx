//////////////////////////////////////////
// This file contains a simple filter
// skeleton you can use to get started.
// With no changes it simply passes
// frames through.

#include "VapourSynth.h"
#include <cstdlib>
#include <algorithm>

typedef struct {
    VSNodeRef* node;
    const VSVideoInfo* vi;
} FilterData;

static void VS_CC filterInit(VSMap* in, VSMap* out, void** instanceData, VSNode* node, VSCore* core, const VSAPI* vsapi) {
    FilterData* d = (FilterData*)*instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
}

static const VSFrameRef* VS_CC filterGetFrame(int n, int activationReason, void** instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
    FilterData* d = (FilterData*)*instanceData;
    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    }
    else if (activationReason == arAllFramesReady) {
        const VSFrameRef* frame = vsapi->getFrameFilter(n, d->node, frameCtx);
        auto fi = d->vi->format;
        auto height = vsapi->getFrameHeight(frame, 0);
        auto width = vsapi->getFrameWidth(frame, 0);
        auto dst = vsapi->newVideoFrame(fi, width, height, frame, core);
        for (auto plane = 0; plane < fi->numPlanes; plane++) {
            auto srcp = reinterpret_cast<const float*>(vsapi->getReadPtr(frame, plane));
            auto dstp = reinterpret_cast<float*>(vsapi->getWritePtr(dst, plane));
            auto h = vsapi->getFrameHeight(frame, plane);
            auto w = vsapi->getFrameWidth(frame, plane);
            auto gauss = [&](auto y, auto x) {
                auto clamp = [&](auto val, auto bound) {
                    return std::min(std::max(val, 0), bound - 1);
                };
                auto above = srcp + clamp(y - 1, h) * w;
                auto current = srcp + y * w;
                auto below = srcp + clamp(y + 1, h) * w;
                auto conv = above[clamp(x - 1, w)] + above[x] * 2 + above[clamp(x + 1, w)] +
                    current[clamp(x - 1, w)] * 2 + current[x] * 4 + current[clamp(x + 1, w)] * 2 +
                    below[clamp(x - 1, w)] + below[x] * 2 + below[clamp(x + 1, w)];
                return conv / 16;
            };
            for (auto y = 0; y < h; y++)
                for (auto x = 0; x < w; x++)
                    (dstp + y * w)[x] = gauss(y, x);
        }
        vsapi->freeFrame(frame);
        return dst;
    }
    return 0;
}

static void VS_CC filterFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    FilterData* d = (FilterData*)instanceData;
    vsapi->freeNode(d->node);
    free(d);
}

static void VS_CC filterCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi) {
    FilterData d;
    FilterData* data;

    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = vsapi->getVideoInfo(d.node);

    data = (FilterData*)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "Filter", filterInit, filterGetFrame, filterFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.debug.gauss", "testc", "filters written with C API", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("GaussBlur", "clip:clip;", filterCreate, 0, plugin);
}
