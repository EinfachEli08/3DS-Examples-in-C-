#include <iostream>
#include <fstream>
#include <cstring>

#include <3ds.h>

u8* inaddr;
u8* outaddr;

void mvd_colorconvert()
{
    Result ret;

    FILE *f = NULL;

    u8* bufAdr = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    u8* gfxtopadr = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);

    MVDSTD_Config config;

    std::cout << "mvd color-format-conversion example.\n";

    f = fopen("sdmc:/mvd_indata.bin", "r");
    if (f)
    {
        fread(inaddr, 1, 0x46500, f);
        fclose(f);
    }
    else
    {
        memcpy(inaddr, bufAdr, 320 * 240 * 3);
    }

    memset(gfxtopadr, 0, 0x46500);
    GSPGPU_FlushDataCache(inaddr, 0x46500);
    GSPGPU_FlushDataCache(gfxtopadr, 0x46500);

    ret = mvdstdInit(MVDMODE_COLORFORMATCONV, MVD_INPUT_YUYV422, MVD_OUTPUT_BGR565, 0, NULL);
    std::cout << "mvdstdInit(): 0x" << std::hex << ret << "\n";

    if (ret == 0)
    {
        mvdstdGenerateDefaultConfig(&config, 320, 240, 320, 240, (u32*)inaddr, (u32*)outaddr, (u32*)&outaddr[0x12c00]);

        ret = mvdstdConvertImage(&config);
        std::cout << "mvdstdConvertImage(): 0x" << std::hex << ret << "\n";
    }

    f = fopen("sdmc:/mvd_outdata.bin", "w");
    if (f)
    {
        fwrite(outaddr, 1, 0x100000, f);
        fclose(f);
    }

    memcpy(gfxtopadr, outaddr, 0x46500);

    mvdstdExit();

    gfxFlushBuffers();
    gfxSwapBuffersGpu();
    gspWaitForVBlank();
}

void mvd_video()
{
    Result ret;
    size_t video_size, nalunitsize;
    u32 video_pos = 0;
    u32 cur_nalunit_pos = 0, prev_nalunit_pos = 0;
    u32 nalcount = 0;
    u8 *video;

    u32 flagval = 0;

    FILE *f = NULL;

    u8* gfxtopadr = NULL;

    MVDSTD_Config config;

    u32 prefix_offset;
    u8 prefix[4] = {0x00, 0x00, 0x00, 0x01};

    std::cout << "Loading video...\n";

    // This loads the entire video into memory, normally you'd use a library to stream it.
    f = fopen("romfs:/video.h264", "r");
    if (f == NULL)
    {
        std::cout << "Failed to open the video in romfs.\n";
        return;
    }

    video = &inaddr[0x100000];
    video_size = fread(video, 1, 0xF00000, f);
    fclose(f);

    if (video_size == 0 || video_size >= 0xF00000)
    {
        std::cout << "Failed to read video / video is too large.\n";
        return;
    }

    ret = mvdstdInit(MVDMODE_VIDEOPROCESSING, MVD_INPUT_H264, MVD_OUTPUT_BGR565, MVD_DEFAULT_WORKBUF_SIZE, NULL);
    std::cout << "mvdstdInit(): 0x" << std::hex << ret << "\n";
    if (ret != 0)
        return;

    std::cout << "Processing 0x" << std::hex << video_size << "-byte video...\n";

    mvdstdGenerateDefaultConfig(&config, 240, 400, 240, 400, NULL, (u32*)outaddr, (u32*)outaddr);

    while (video_pos < video_size + 1)
    {
        cur_nalunit_pos = video_pos;
        video_pos++;

        prefix_offset = 1;

        if (cur_nalunit_pos < video_size)
        {
            if (memcmp(&video[cur_nalunit_pos], prefix, 4))
            {
                continue;
            }
            else
            {
                video_pos++;
            }
        }

        if (nalcount && prev_nalunit_pos != cur_nalunit_pos)
        {
            nalunitsize = cur_nalunit_pos - prev_nalunit_pos - prefix_offset;
            if (nalunitsize > 0x100000)
            {
                std::cout << "The NAL-unit at offset 0x" << std::hex << nalunitsize << " is too large.\n";
                break;
            }

            memcpy(inaddr, &video[prev_nalunit_pos + prefix_offset], nalunitsize);
            GSPGPU_FlushDataCache(inaddr, nalunitsize);

            MVDSTD_ProcessNALUnitOut tmpout;

            ret = mvdstdProcessVideoFrame(inaddr, nalunitsize, flagval, &tmpout);
            if (!MVD_CHECKNALUPROC_SUCCESS(ret))
            {
                std::cout << "mvdstdProcessVideoFrame() at NAL-unit offset 0x" << std::hex << prev_nalunit_pos << " size 0x" << nalunitsize << " returned: 0x" << ret << ". remaining_size=0x" << tmpout.remaining_size << "\n";
                break;
            }

            if (ret != MVD_STATUS_PARAMSET && ret != MVD_STATUS_INCOMPLETEPROCESSING)
            {
                gfxtopadr = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
                config.physaddr_outdata0 = osConvertVirtToPhys(gfxtopadr);

                ret = mvdstdRenderVideoFrame(&config, true);
                if (ret != MVD_STATUS_OK)
                {
                    std::cout << "mvdstdRenderVideoFrame() at NAL-unit offset 0x" << std::hex << prev_nalunit_pos << " returned: 0x" << ret << "\n";
                    break;
                }

                // Add input handling and buffer swapping here as in the original code

                gfxSwapBuffersGpu();
            }
        }

        nalcount++;

        prev_nalunit_pos = cur_nalunit_pos;
    }

    mvdstdExit();
}

int main()
{
    Result ret = 0;

    int draw = 1;
    int ready = 0;
    int type = 0;

    gfxInit(GSP_RGB565_OES, GSP_BGR8_OES, false);
    consoleInit(GFX_BOTTOM, NULL);

    ret = romfsInit();
    if (R_FAILED(ret))
    {
        std::cout << "romfsInit() failed: 0x" << std::hex << ret << "\n";
        ready = -1;
    }

    if (ready == 0)
    {
        inaddr = (u8*)linearMemAlign(0x1000000, 0x40);
        outaddr = (u8*)linearMemAlign(0x100000, 0x40);

        if (!(inaddr && outaddr))
        {
            ready = -2;
            std::cout << "Failed to allocate memory.\n";
        }
    }

    // Main loop
    while (aptMainLoop())
    {
        gspWaitForVBlank();
        hidScanInput();

        if (draw && type == 0)
        {
            consoleClear();
            draw = 0;

            if (ready == 0)
                std::cout << "mvd example\n";

            std::cout << "Press START to exit.\n";
            if (ready == 0)
            {
                std::cout << "Press A for color-format-conversion.\n";
                std::cout << "Press B for video(no sound).\n";
            }
        }

        u32 kDown = hidKeysDown();

        if (type)
        {
            if (kDown & KEY_A)
            {
                type = 0;
                continue;
            }
        }

        if (type)
            continue;

        if (kDown & KEY_START)
            break;

        if (ready == 0)
        {
            type = 0;
            if (kDown & KEY_A)
                type = 1;
            if (kDown & KEY_B)
                type = 2;

            if (type)
            {
                memset(inaddr, 0, 0x100000);
                memset(outaddr, 0, 0x100000);

                if (type == 1)
                    mvd_colorconvert();
                if (type == 2)
                    mvd_video();

                draw = 1;
                std::cout << "Press A to continue.\n";
            }
        }
    }

    if (inaddr)
        linearFree(inaddr);
    if (outaddr)
        linearFree(outaddr);

    if (ready != -1)
        romfsExit();

    gfxExit();
    return 0;
}
