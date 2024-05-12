#include <iostream>
#include <3ds.h>

int main()
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    std::cout << "APT chainload example\n";
    std::cout << "Press A to chainload to EUR camera app\n";
    std::cout << "Press START to exit\n";

    while (aptMainLoop())
    {
        gspWaitForVBlank();
        gfxSwapBuffers();
        hidScanInput();

        u32 kDown = hidKeysDown();

        if (kDown & KEY_START)
            break;

        if (kDown & KEY_A) // If the A button got pressed, start the app launch
        {
            aptSetChainloader(0x0004001000022400LL, 0); // *EUR* camera app title ID
            break;
        }
    }

    gfxExit();
    return 0;
}
