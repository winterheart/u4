/*
 * imagemgr.cpp
 */

#include <string.h>

#include "config.h"
#include "error.h"
#include "imageloader.h"
#include "imagemgr.h"
#include "intro.h"
#include "settings.h"
#include "xu4.h"
#include "gpu.h"

using std::string;


ImageSymbols ImageMgr::sym;

ImageMgr::ImageMgr() :
    vgaColors(NULL), greyColors(NULL), visionBuf(NULL),
    resGroup(0) {

    xu4.config->internSymbols(&sym.tiles, 45,
        "tiles charset borders title options_top\n"
        "options_btm tree portal outside inside\n"
        "wagon gypsy abacus honcom valjus\n"
        "sachonor spirhum beasties key honesty\n"
        "compassn valor justice sacrific honor\n"
        "spirit humility truth love courage\n"
        "stoncrcl infinity rune1 rune2 rune3\n"
        "rune4 rune5 rune6 rune7 rune8\n"
        "gemtiles moongate items blackbead whitebead");

    baseSet = xu4.config->newImageSet();

    vgaGraphics = false;
    if (baseSet) {
        std::map<Symbol, ImageInfo *>::iterator it =
            baseSet->info.find(sym.tiles);
        if (it != baseSet->info.end()) {
            string fname = it->second->getFilename();
            if (fname.find(".vga") != string::npos)
                vgaGraphics = true;
        }
    }

#if 0
    // Dump images.
    xu4.imageMgr = this;
    char str[24];
    Symbol* stable = &sym.key;
    for (int i = 0; 1; ++i) {
        ImageInfo* info = get(stable[i]);
        if (info && info->image) {
            sprintf(str, "/tmp/img%02d_%s.ppm", i,
                    xu4.config->symbolName(info->name));
            info->image->save(str);
        }
        if (stable[i] == sym.whitebead)
            break;
    }
#endif
}

ImageMgr::~ImageMgr() {
    delete baseSet;
    delete[] vgaColors;
    delete[] greyColors;
    delete[] visionBuf;
}

void ImageMgr::fixupIntro(Image *im) {
    const unsigned char *sigData;
    int i, x, y;
    RGBA color;

    sigData = xu4.intro->getSigData();
    {
        /* ----------------------------
         * update the position of "and"
         * ---------------------------- */
        im->drawSubRectOn(im, 148, 17, 153, 17, 11, 4);
        im->drawSubRectOn(im, 159, 17, 165, 18,  1, 4);
        im->drawSubRectOn(im, 160, 17, 164, 17, 16, 4);
        /* ---------------------------------------------
         * update the position of "Origin Systems, Inc."
         * --------------------------------------------- */
        im->drawSubRectOn(im,  86, 21,  88, 21, 114, 9);
        im->drawSubRectOn(im, 199, 21, 202, 21,   6, 9);
        im->drawSubRectOn(im, 207, 21, 208, 21,  28, 9);
        /* ---------------------------------------------
         * update the position of "Ultima IV"
         * --------------------------------------------- */
        // move this *prior* to moving "present"
        im->drawSubRectOn(im, 59, 33, 61, 33, 204, 46);
#if 0
        /*
         * NOTE: This just tweaks the kerning by a few pixels.  With the Image
         * rewrite for RGBA8-only images, "THE" disappears (dest. X must be
         * less than source X when an image blits onto itself).
         *
         * If the goal is to preserve the original experience then the image
         * should not be touched anyway, and a "recreated" experience will
         * have correct input images.
         */

        /* ---------------------------------------------
         * update the position of "Quest of the Avatar"
         * --------------------------------------------- */
        im->drawSubRectOn(im,  69, 80,  70, 80, 11, 13);  // quEst
        im->drawSubRectOn(im,  82, 80,  84, 80, 27, 13);  // queST
        im->drawSubRectOn(im, 131, 80, 132, 80, 11, 13);  // oF
        im->drawSubRectOn(im, 150, 80, 149, 80, 40, 13);  // THE
        im->drawSubRectOn(im, 166, 80, 165, 80, 11, 13);  // tHe
        im->drawSubRectOn(im, 200, 80, 201, 80, 81, 13);  // AVATAR
        im->drawSubRectOn(im, 227, 80, 228, 80, 11, 13);  // avAtar
#endif
    }
    /* -----------------------------------------------------------------------------
     * copy "present" to new location between "Origin Systems, Inc." and "Ultima IV"
     * ----------------------------------------------------------------------------- */
    // do this *after* moving "Ultima IV"
    im->drawSubRectOn(im, 132, 33, 135, 0, 56, 5);

    /* ----------------------------
     * erase the original "present"
     * ---------------------------- */
    im->fillRect(135, 0, 56, 5, 0, 0, 0);

    /* -------------------------
     * update the colors for VGA
     * ------------------------- */
    ImageInfo* borders = ImageMgr::get(BKGD_BORDERS);
    if (borders && (borders->getFilename().compare(0, 4, "u4u/") == 0))
    {
        // update the border appearance
        borders->image->drawSubRectOn(im, 0, 96, 0, 0, 16, 56);
        for (int i=0; i < 9; i++)
        {
            borders->image->drawSubRectOn(im, 16+(i*32), 96, 144, 0, 48, 48);
        }
        im->drawSubRectInvertedOn(im, 0, 144, 0, 104, 320, 40);
        im->drawSubRectOn(im, 0, 184, 0, 96, 320, 8);
    }

    /* -----------------------------
     * draw "Lord British" signature
     * ----------------------------- */
    color = im->setColor(0, 255, 255);  // cyan for EGA
    int blue[16] = {255, 250, 226, 226, 210, 194, 161, 161,
                    129,  97,  97,  64,  64,  32,  32,   0};
    i = 0;
    while (sigData[i] != 0) {
        /* (x/y) are unscaled coordinates, i.e. in 320x200 */
        x = sigData[i] + 0x14;
        y = 0xBF - sigData[i+1];

        if (vgaGraphics)
        {
            // yellow gradient
            color = im->setColor(255, (y == 1 ? 250 : 255), blue[y]);
        }

        im->fillRect(x, y, 2, 1, color.r, color.g, color.b);
        i += 2;
    }

    /* --------------------------------------------------------------
     * draw the red line between "Origin Systems, Inc." and "present"
     * -------------------------------------------------------------- */
    /* we're still working with an unscaled surface */
    if (vgaGraphics)
        color = im->setColor(0, 0, 161);    // dark blue
    else
        color = im->setColor(128, 0, 0);    // dark red for EGA

    for (i = 84; i < 236; i++)  // 152 px wide
        im->fillRect(i, 31, 1, 1, color.r, color.g, color.b);
}

/*
 * Each VGA vision component must be XORed with all the previous
 * vision components to get the actual image.
 */
void ImageMgr::fixupAbyssVision(Image32* img) {
    const RGBA* palette = vgaPalette();
    RGBA* cp = (RGBA*) img->pixels;
    int n = img->w * img->h;
    int i, ci;

    if (visionBuf) {
        for (i = 0; i < n; ++i) {
            ci = cp->r ^ visionBuf[i];
            visionBuf[i] = ci;
            *cp++ = palette[ci];
        }
    } else {
        visionBuf = new uint8_t[n];
        for (i = 0; i < n; ++i) {
            ci = cp->r;
            visionBuf[i] = ci;
            *cp++ = palette[ci];
        }
    }
}

void ImageMgr::fixupTransparent(Image* img, RGBA color) {
    uint32_t* it  = img->pixels;
    uint32_t* end = it + (img->w * img->h);
    uint32_t ucol, trans;

    ucol = *((uint32_t*) &color);
    color.a = 0;
    trans = *((uint32_t*) &color);

    while (it != end) {
        if (*it == ucol)
            *it = trans;
        ++it;
    }
}

static void swapColors(Image32* img, const RGBA* colorA, const RGBA* colorB) {
    uint32_t* it  = img->pixels;
    uint32_t* end = it + (img->w * img->h);
    uint32_t ua, ub;

    ua = *((uint32_t*) colorA);
    ub = *((uint32_t*) colorB);

    while (it != end) {
        if (*it == ua)
            *it = ub;
        else if (*it == ub)
            *it = ua;
        ++it;
    }
}

void ImageMgr::fixupAbacus(Image *im) {

    /*
     * surround each bead with a row green pixels to avoid artifacts
     * when scaling
     */

    im->fillRect( 7, 186, 1, 14, 0, 255, 80); /* green */
    im->fillRect(16, 186, 1, 14, 0, 255, 80); /* green */
    im->fillRect( 8, 186, 8,  1, 0, 255, 80); /* green */
    im->fillRect( 8, 199, 8,  1, 0, 255, 80); /* green */

    im->fillRect(23, 186, 1, 14, 0, 255, 80); /* green */
    im->fillRect(32, 186, 1, 14, 0, 255, 80); /* green */
    im->fillRect(24, 186, 8,  1, 0, 255, 80); /* green */
    im->fillRect(24, 199, 8,  1, 0, 255, 80); /* green */

    if (vgaGraphics) {
        RGBA light, dark;
        rgba_set(light, 0x55, 0xff, 0x50, 0xff);
        rgba_set(dark,  0x58, 0x8d, 0x43, 0xff);
        swapColors(im, &light, &dark);
    }
}

/**
 * Swap blue and green for the dungeon walls when facing north or
 * south.
 */
void ImageMgr::fixupDungNS(Image *im) {
    RGBA blue, green;
    rgba_set(blue,  0, 0, 0x80, 0xff);
    rgba_set(green, 0, 0x80, 0, 0xff);
    swapColors(im, &blue, &green);
}

/**
 * The FMTowns images have a different screen dimension. This moves them up to what xu4 is accustomed to.
 * south.
 */
void ImageMgr::fixupFMTowns(Image *im) {
    for (int y = 20; y < im->height(); y++) {
        for (int x = 0; x < im->width(); x++) {
            unsigned int index;
            im->getPixelIndex(x, y, index);
            im->putPixelIndex(x, y-20, index);
        }
    }
}

#define isImageChunkId(fn)  (fn[0] == 'I' && fn[1] == 'M' && fn[4] == '\0')

U4FILE * ImageMgr::getImageFile(ImageInfo *info)
{
    U4FILE *file;
    const char* fn = xu4.config->confString(info->filename);

    if (strncmp(fn, "u4/", 3) == 0) {
        // Original game data; strip off path.
        file = u4fopen(string(fn + 3));
    } else if(strncmp(fn, "u4u/", 4) == 0) {
        // Upgrade game data; strip off path.
        file = u4fopen_upgrade(string(fn + 4));
#ifdef CONF_MODULE
    } else if (isImageChunkId(fn)) {
        const CDIEntry* ent = xu4.config->imageFile(fn);
        if (ent) {
            file = u4fopen_stdio(xu4.config->modulePath(ent));
            u4fseek(file, ent->offset, SEEK_SET);
        } else
            file = NULL;
    } else
        file = NULL;
#else
    } else {
        string filename(fn);
        string pathname(u4find_graphics(filename));
        if (pathname.empty())
            file = NULL;
        else
            file = u4fopen_stdio(pathname.c_str());
    }
#endif
    return file;
}

ImageInfo* ImageMgr::imageInfo(Symbol name, const SubImage** subPtr) {
    const SubImage* subImg = NULL;
    ImageInfo* info = get(name);
    if (! info) {
        subImg = getSubImage(name, &info);
        if (subImg) {
            if (! info->image)
                info = load(info);
        }
    }
    *subPtr = subImg;
    return info;
}

/**
 * Load an image.
 * Return ImageInfo with image pointer set or NULL if load failed.
 */
ImageInfo *ImageMgr::get(Symbol name) {
    if (! baseSet)
        return NULL;

    std::map<Symbol, ImageInfo *>::iterator it = baseSet->info.find(name);
    if (it == baseSet->info.end())
        return NULL;

    ImageInfo* info = it->second;
    if (! info)
        return NULL;

    /* return if already loaded */
    if (info->image != NULL)
        return info;

    return load(info);
}

#ifdef CONF_MODULE
static Image* buildAtlas(ImageMgr* mgr, ImageInfo* atlas) {
    const int maxChild = 16;
    AtlasSubImage asiBuffer[maxChild];
    const ImageInfo* subInfo[maxChild];
    const ImageInfo* info;
    RGBA brush;
    int i, n;
    int siCount = 0;
    int count = xu4.config->atlasImages(atlas->filename, asiBuffer, maxChild);
    Image* image = Image::create(atlas->width, atlas->height);

    rgba_set(brush, 255, 0, 255, 255);

    // Blit the child images and count the total number of SubImages.
    for (i = 0; i < count; ++i) {
        const AtlasSubImage* asi = asiBuffer + i;
        if (asi->name < AEDIT_OP_COUNT) {
            subInfo[i] = NULL;
            switch (asi->name) {
                case AEDIT_BRUSH:
                    rgba_set(brush, asi->x, asi->y, asi->w, asi->h);
                    break;
                case AEDIT_RECT:
                    image32_fillRect(image, asi->x, asi->y, asi->w, asi->h,
                                     &brush);
                    break;
            }
        } else {
            subInfo[i] = info = mgr->get(asi->name);
            if (info && info->image) {
                image32_blit(image, asi->x, asi->y, info->image, 0);

                n = info->subImageCount;
                if (! n)
                    n = info->tiles;
                siCount += n;
            }
        }
    }

    // Merge and adjust all SubImages for the atlas.
    if (siCount) {
        const SubImage* it;
        const SubImage* end;
        SubImage* sid = new SubImage[siCount];

        atlas->subImageCount = siCount;
        atlas->subImages = sid;

        for (i = 0; i < count; ++i) {
            info = subInfo[i];
            if (! info)
                continue;

            if (info->subImageCount) {
                it  = info->subImages;
                end = it + info->subImageCount;
                while (it != end) {
                    *sid = *it++;
                    sid->x += asiBuffer[i].x;
                    sid->y += asiBuffer[i].y;

                    atlas->subImageIndex[sid->name] = sid - atlas->subImages;
                    /*
                    printf("KR atlas si %d %d,%d %s\n",
                           int(sid - atlas->subImages), sid->x, sid->y,
                           xu4.config->symbolName(sid->name));
                    */
                    ++sid;
                }
            } else if (info->tiles) {
                // Create SubImages for unnamed tiles.
                // NOTE: This code assumes the image is one tile wide.
                int tileDim = info->width;
                int tileY = asiBuffer[i].y;
                for (n = 0; n < info->tiles; ++n) {
                    sid->x      = asiBuffer[i].x;
                    sid->y      = tileY;
                    sid->width  = tileDim;
                    sid->height = tileDim;

                    if (n) {
                        sid->name = SYM_UNSET;
                        sid->celCount = 0;
                    } else {
                        sid->name = info->name;
                        atlas->subImageIndex[info->name] = sid - atlas->subImages;
                        sid->celCount = info->tiles;
                    }
                    ++sid;

                    tileY += tileDim;
                }
            }
        }

        // Compute UVs.
        if (! atlas->tileTexCoord) {
            float* uv;
            float iwf = (float) atlas->width;
            float ihf = (float) atlas->height;
            atlas->tileTexCoord = uv = new float[siCount * 4];
            it = (SubImage*) atlas->subImages;
            for (i = 0; i < siCount; ++i) {
                *uv++ = it->x / iwf;
                *uv++ = it->y / ihf;
                *uv++ = (it->x + it->width) / iwf;
                *uv++ = (it->y + it->height) / ihf;
                ++it;
            }
        }

        atlas->tex = gpu_makeTexture(image);
    }
    return image;
}
#endif

ImageInfo* ImageMgr::load(ImageInfo* info) {
#ifdef CONF_MODULE
    if (info->filetype == FTYPE_ATLAS) {
        info->image = buildAtlas(this, info);
        info->resGroup = resGroup;
        return info;
    }
#endif

    U4FILE *file = getImageFile(info);
    Image *unscaled = NULL;
    if (file) {
        //printf( "ImageMgr load %d:%s\n", resGroup, info->filename.c_str() );

        unscaled = loadImage(file, info->filetype, info->width, info->height,
                             (info->fixup == FIXUP_ABYSS) ? BPP_CLUT8
                                                          : info->depth);
        u4fclose(file);

        if (! unscaled) {
            errorWarning("Can't load image \"%s\" with type %d",
                         xu4.config->confString(info->filename), info->filetype);
            return info;
        }

        info->resGroup = resGroup;
        if (info->width == -1) {
            // Write in the values for later use.
            info->width  = unscaled->width();
            info->height = unscaled->height();
        }

        // Pre-compute tile UVs.
        if (info->tiles > 1 && info->tileTexCoord == NULL ) {
            // Assuming image is one tile wide.
            float iwf = (float) unscaled->width();
            float ihf = (float) unscaled->height();
            float tileH = iwf;
            float tileY = 0.0f;
            float *uv;
            int tileCount = info->tiles;

            info->tileTexCoord = uv = new float[tileCount * 4];
            for (int i = 0; i < tileCount; ++i) {
                *uv++ = 0.0f;
                *uv++ = tileY / ihf;
                *uv++ = 1.0f;
                *uv++ = (tileY + tileH) / ihf;
                tileY += tileH;
            }
        }
        /*
        SubImage* simg = (SubImage*) info->subImages;
        SubImage* end = simg + info->subImageCount;
        while (simg != end) {
            simg->u0 = simg->x / iwf;
            simg->v0 = simg->y / ihf;
            simg->u1 = (simg->x + simg->width) / iwf;
            simg->v1 = (simg->y + simg->height) / ihf;
            ++simg;
        }
        */

#if 0
        string out("/tmp/xu4/");
        out.append(xu4.config->symbolName(info->name));
        unscaled->save(out.append(".ppm").c_str());
#endif
    }
    else
    {
        errorWarning("Failed to open file %s for reading.",
                     xu4.config->confString(info->filename));
        return NULL;
    }

    if (unscaled == NULL)
        return NULL;

    /*
     * fixup the image before scaling it
     */
    switch (info->fixup) {
    case FIXUP_NONE:
        break;
    case FIXUP_INTRO:
        fixupIntro(unscaled);
        break;
    case FIXUP_ABYSS:
        fixupAbyssVision(unscaled);
        break;
    case FIXUP_ABACUS:
        fixupAbacus(unscaled);
        break;
    case FIXUP_DUNGNS:
        fixupDungNS(unscaled);
        break;
    case FIXUP_FMTOWNSSCREEN:
        fixupFMTowns(unscaled);
        break;
    case FIXUP_TRANSPARENT0:
        fixupTransparent(unscaled, Image::black);
        break;
    case FIXUP_BLACKTRANSPARENCYHACK:
        //Apply transparency shadow hack to ultima4 ega and vga upgrade classic graphics.
        if (xu4.settings->enhancements &&
            xu4.settings->enhancementsOptions.u4TileTransparencyHack)
        {
            int transparency_shadow_size =xu4.settings->enhancementsOptions.u4TrileTransparencyHackShadowBreadth;
            int opacity = xu4.settings->enhancementsOptions.u4TileTransparencyHackPixelShadowOpacity;

            // NOTE: The first 16 tiles are landscape and must be fully opaque!
            int f = (info->name == BKGD_SHAPES) ? 16 : 0;
            int frames = info->tiles;
            for ( ; f < frames; ++f) {
                if (f == 126)
                    continue;   // Skip tile_black
                unscaled->performTransparencyHack(Image::black, frames, f, transparency_shadow_size, opacity);
            }
        }
        break;
    }

#if 0
    string out2("/tmp/xu4/");
    out2.append(xu4.config->symbolName(info->name));
    unscaled->save(out2.append("-fixup.ppm").c_str());
#endif

    info->image = unscaled;
    //info->tex = gpu_makeTexture(info->image);

    return info;
}

/**
 * Returns information for the given image set.
 */
const SubImage* ImageMgr::getSubImage(Symbol name, ImageInfo** infoPtr) {
    std::map<Symbol, ImageInfo *>::iterator it;
    foreach (it, baseSet->info) {
        ImageInfo *info = (ImageInfo *) it->second;
        std::map<Symbol, int>::iterator j = info->subImageIndex.find(name);
        if (j != info->subImageIndex.end()) {
            *infoPtr = info;
            return info->subImages + j->second;
        }
    }
    return NULL;
}

/**
 * Set the group loaded images will belong to.
 * Return the previously set group.
 */
uint16_t ImageMgr::setResourceGroup(uint16_t group) {
    uint16_t prev = resGroup;
    resGroup = group;
    return prev;
}

/**
 * Free all images that are part of the specified group.
 */
void ImageMgr::freeResourceGroup(uint16_t group) {
    std::map<Symbol, ImageInfo *>::iterator j;

    foreach (j, baseSet->info) {
        ImageInfo *info = j->second;
        if (info->image && (info->resGroup == group)) {
            //printf("ImageMgr::freeRes %s\n", info->filename.c_str());

            if (info->tex) {
                gpu_freeTexture(info->tex);
                info->tex = 0;
            }

            delete info->image;
            info->image = NULL;
        }
    }
}

/**
 * Get the 256 color VGA palette from the u4upgrad file.
 */
const RGBA* ImageMgr::vgaPalette() {
    if (vgaColors == NULL) {
        U4FILE *pal = u4fopen_upgrade("u4vga.pal");
        if (!pal)
            return NULL;

        vgaColors = new RGBA[256];

        for (int i = 0; i < 256; i++) {
            vgaColors[i].r = u4fgetc(pal) * 255 / 63;
            vgaColors[i].g = u4fgetc(pal) * 255 / 63;
            vgaColors[i].b = u4fgetc(pal) * 255 / 63;
            vgaColors[i].a = 255;
        }
        u4fclose(pal);
    }
    return vgaColors;
}

/**
 * Return a palette where color is equal to CLUT index.
 */
const RGBA* ImageMgr::greyPalette() {
    if (! greyColors) {
        greyColors = new RGBA[256];
        for (int i = 0; i < 256; i++) {
            greyColors[i].r = greyColors[i].g = greyColors[i].b = i;
            greyColors[i].a = 255;
        }
    }
    return greyColors;
}

ImageSet::~ImageSet() {
    std::map<Symbol, ImageInfo *>::iterator it;
    foreach (it, info)
        delete it->second;
}

ImageInfo::ImageInfo() {
    tex = 0;
    tileTexCoord = NULL;
    subImageCount = 0;
    subImages = NULL;
}

ImageInfo::~ImageInfo() {
    delete[] subImages;
    delete image;
    if (tex)
        gpu_freeTexture(tex);
    delete[] tileTexCoord;
}

string ImageInfo::getFilename() const {
    return xu4.config->confString(filename);
}
