#include "Icons.h"
#include <glad/gl.h>
#include <string>
#include <vector>

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

namespace {

#define STROKE R"(fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round")"
#define SVG(body) R"(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">)" body "</svg>"

const char* kSvg[(int)Icon::Count] = {
    /* Move      */ SVG(R"(<path d="M12 3v18M3 12h18M9 6l3-3 3 3M9 18l3 3 3-3M6 9l-3 3 3 3M18 9l3 3-3 3" )" STROKE R"(/>)"),
    /* Rotate    */ SVG(R"(<path d="M20 12a8 8 0 1 1-2.6-5.9M20 3.5v5h-5" )" STROKE R"(/>)"),
    /* Scale     */ SVG(R"(<rect x="3.5" y="10.5" width="10" height="10" )" STROKE R"(/><path d="M14 4h6v6M20 4l-8 8" )" STROKE R"(/>)"),
    /* Play      */ SVG(R"(<path d="M7 4l13 8-13 8z" fill="#fff"/>)"),
    /* Pause     */ SVG(R"(<rect x="6" y="4" width="4" height="16" fill="#fff"/><rect x="14" y="4" width="4" height="16" fill="#fff"/>)"),
    /* Step      */ SVG(R"(<path d="M5 4l10 8-10 8z" fill="#fff"/><rect x="17" y="4" width="3" height="16" fill="#fff"/>)"),
    /* Stop      */ SVG(R"(<rect x="5" y="5" width="14" height="14" fill="#fff"/>)"),
    /* Cube      */ SVG(R"(<path d="M12 2l9 5v10l-9 5-9-5V7zM3 7l9 5 9-5M12 12v10" )" STROKE R"(/>)"),
    /* Sphere    */ SVG(R"(<circle cx="12" cy="12" r="9" )" STROKE R"(/><ellipse cx="12" cy="12" rx="9" ry="3.5" )" STROKE R"(/><ellipse cx="12" cy="12" rx="3.5" ry="9" )" STROKE R"(/>)"),
    /* Plane     */ SVG(R"(<path d="M3 17l5-10h13l-5 10z" )" STROKE R"(/>)"),
    /* Light     */ SVG(R"(<path d="M9 18h6M10 21h4M12 3a6 6 0 0 0-3.5 10.9c.6.5 1 1.2 1 2.1h5c0-.9.4-1.6 1-2.1A6 6 0 0 0 12 3z" )" STROKE R"(/>)"),
    /* Folder    */ SVG(R"(<path d="M3 6h6l2 2h10v11H3z" )" STROKE R"(/>)"),
    /* File      */ SVG(R"(<path d="M6 3h8l5 5v13H6zM14 3v5h5" )" STROKE R"(/>)"),
    /* Eye       */ SVG(R"(<path d="M2 12s4-7 10-7 10 7 10 7-4 7-10 7S2 12 2 12z" )" STROKE R"(/><circle cx="12" cy="12" r="3" )" STROKE R"(/>)"),
    /* Trash     */ SVG(R"(<path d="M4 7h16M9 7V4h6v3M6 7l1 14h10l1-14" )" STROKE R"(/>)"),
    /* Copy      */ SVG(R"(<rect x="8" y="8" width="12" height="12" )" STROKE R"(/><path d="M16 8V4H4v12h4" )" STROKE R"(/>)"),
    /* Plus      */ SVG(R"(<path d="M12 5v14M5 12h14" )" STROKE R"(/>)"),
    /* Hierarchy */ SVG(R"(<rect x="9" y="3" width="6" height="5" )" STROKE R"(/><rect x="3" y="16" width="6" height="5" )" STROKE R"(/><rect x="15" y="16" width="6" height="5" )" STROKE R"(/><path d="M12 8v4M6 16v-4h12v4" )" STROKE R"(/>)"),
    /* Inspector */ SVG(R"(<path d="M4 6h16M4 12h16M4 18h10" )" STROKE R"(/><circle cx="16" cy="6" r="2" fill="#fff"/><circle cx="9" cy="12" r="2" fill="#fff"/>)"),
    /* Console   */ SVG(R"(<rect x="3" y="4" width="18" height="16" rx="2" )" STROKE R"(/><path d="M7 10l3 2-3 2M12 15h5" )" STROKE R"(/>)"),
    /* Gear      */ SVG(R"(<circle cx="12" cy="12" r="3" )" STROKE R"(/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.1 2.1M17 17l2.1 2.1M4.9 19.1L7 17M17 7l2.1-2.1" )" STROKE R"(/>)"),
    /* Anvil     */ SVG(R"(<path d="M2 5h15l5 3.5h-5c-1 3-3 4.5-5.5 4.5H11v3h4v3H6v-3h4v-3C7 13 5.5 11 5 8.5H2z" fill="#fff"/>)"),
};

unsigned gTex[(int)Icon::Count] = {};

} // namespace

namespace icons {

void init() {
    const int px = 64;   // rasterized at 64 px, drawn at ~16-24 px: stays crisp on HiDPI
    NSVGrasterizer* rast = nsvgCreateRasterizer();
    for (int i = 0; i < (int)Icon::Count; ++i) {
        std::string src = kSvg[i];
        NSVGimage* img = nsvgParse(src.data(), "px", 96.0f);
        if (!img) continue;
        std::vector<unsigned char> buf((size_t)px * px * 4, 0);
        nsvgRasterize(rast, img, 0, 0, (float)px / 24.0f, buf.data(), px, px, px * 4);
        nsvgDelete(img);
        glCreateTextures(GL_TEXTURE_2D, 1, &gTex[i]);
        glTextureStorage2D(gTex[i], 5, GL_RGBA8, px, px);
        glTextureSubImage2D(gTex[i], 0, 0, 0, px, px, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
        glGenerateTextureMipmap(gTex[i]);
        glTextureParameteri(gTex[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(gTex[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(gTex[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(gTex[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    nsvgDeleteRasterizer(rast);
}

void shutdown() { glDeleteTextures((int)Icon::Count, gTex); }
unsigned texture(Icon i) { return gTex[(int)i]; }

} // namespace icons
