#include "clay/clay.h"
#include "../../renderers/raylib/clay_renderer_raylib.c"

void RenderHeaderButton(Clay_String text) {
    CLAY(
        CLAY_LAYOUT({ .padding = { 16, 8 }}),
        CLAY_RECTANGLE({
            .color = { 140, 140, 140, 255 },
            .cornerRadius = 5
        })
    ) {
    }
}

void RenderDropdownMenuItem(Clay_String text) {
    CLAY(CLAY_LAYOUT({ .padding = { 16, 16 }})) {

    }
}

// This function is new since the video was published
void HandleClayErrors(Clay_ErrorData errorData) {
    printf("%s", errorData.errorText.chars);
}


int main(void) {
    Clay_Raylib_Initialize(1024, 768, "clay-demo", FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT); // Extra parameters to this function are new since the video was published

    uint64_t clayRequiredMemory = Clay_MinMemorySize();
    Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(clayRequiredMemory, malloc(clayRequiredMemory));
    Clay_Initialize(clayMemory, (Clay_Dimensions) {
       .width = GetScreenWidth(),
       .height = GetScreenHeight()
    }, (Clay_ErrorHandler) { HandleClayErrors }); // This final argument is new since the video was published
    Clay_SetMeasureTextFunction(Raylib_MeasureText);

    while (!WindowShouldClose()) {
        // Run once per frame
        Clay_SetLayoutDimensions((Clay_Dimensions) {
                .width = GetScreenWidth(),
                .height = GetScreenHeight()
        });

        Vector2 mousePosition = GetMousePosition();
        Vector2 scrollDelta = GetMouseWheelMoveV();
        Clay_SetPointerState(
            (Clay_Vector2) { mousePosition.x, mousePosition.y },
            IsMouseButtonDown(0)
        );
        Clay_UpdateScrollContainers(
            true,
            (Clay_Vector2) { scrollDelta.x, scrollDelta.y },
            GetFrameTime()
        );

        Clay_Sizing layoutExpand = {
            .width = CLAY_SIZING_GROW({}),
            .height = CLAY_SIZING_GROW({})
        };

        Clay_RectangleElementConfig contentBackgroundConfig = {
            .color = { 90, 90, 90, 255 },
            .cornerRadius = 8
        };

        Clay_BeginLayout();
        // Build UI here
        CLAY(
            CLAY_ID("OuterContainer"),
            CLAY_RECTANGLE({ .color = { 43, 41, 51, 255 } }),
            CLAY_LAYOUT({
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = layoutExpand,
                .padding = { 16, 16 },
                .childGap = 16
            })
        ) {
            // Child elements go inside braces
            CLAY(
                CLAY_ID("HeaderBar"),
                CLAY_RECTANGLE(contentBackgroundConfig),
                CLAY_LAYOUT({
                    .sizing = {
                        .height = CLAY_SIZING_FIXED(60),
                        .width = CLAY_SIZING_GROW({})
                    },
                    .padding = { 16 },
                    .childGap = 16,
                    .childAlignment = {
                        .y = CLAY_ALIGN_Y_CENTER
                    }
                })
            ) {
                // Header buttons go here
                CLAY(
                    CLAY_ID("FileButton"),
                    CLAY_LAYOUT({ .padding = { 16, 8 }}),
                    CLAY_RECTANGLE({
                        .color = { 140, 140, 140, 255 },
                        .cornerRadius = 5
                    })
                ) {
                    bool fileMenuVisible =
                        Clay_PointerOver(Clay_GetElementId(CLAY_STRING("FileButton")))
                        ||
                        Clay_PointerOver(Clay_GetElementId(CLAY_STRING("FileMenu")));

                    if (fileMenuVisible) { // Below has been changed slightly to fix the small bug where the menu would dismiss when mousing over the top gap
                        CLAY(
                            CLAY_ID("FileMenu"),
                            CLAY_FLOATING({
                                .attachment = {
                                    .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM
                                },
                            }),
                            CLAY_LAYOUT({
                                .padding = {0, 8 }
                            })
                        ) {
                            CLAY(
                                CLAY_LAYOUT({
                                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                    .sizing = {
                                            .width = CLAY_SIZING_FIXED(200)
                                    },
                                }),
                                CLAY_RECTANGLE({
                                    .color = { 40, 40, 40, 255 },
                                    .cornerRadius = 8
                                })
                            ) {
                                // Render dropdown items here
                                RenderDropdownMenuItem(CLAY_STRING("New"));
                                RenderDropdownMenuItem(CLAY_STRING("Open"));
                                RenderDropdownMenuItem(CLAY_STRING("Close"));
                            }
                        }
                    }
                }
                RenderHeaderButton(CLAY_STRING("Edit"));
                CLAY(CLAY_LAYOUT({ .sizing = { CLAY_SIZING_GROW({}) }})) {}
                RenderHeaderButton(CLAY_STRING("Upload"));
                RenderHeaderButton(CLAY_STRING("Media"));
                RenderHeaderButton(CLAY_STRING("Support"));
            }

            CLAY(
                CLAY_ID("LowerContent"),
                CLAY_LAYOUT({ .sizing = layoutExpand, .childGap = 16 })
            ) {
                CLAY(
                    CLAY_ID("Sidebar"),
                    CLAY_RECTANGLE(contentBackgroundConfig),
                    CLAY_LAYOUT({
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .padding = { 16, 16 },
                        .childGap = 8,
                        .sizing = {
                            .width = CLAY_SIZING_FIXED(250),
                            .height = CLAY_SIZING_GROW({})
                        }
                    })
                ) {
                }

                CLAY(
                    CLAY_ID("MainContent"),
                    CLAY_RECTANGLE(contentBackgroundConfig),
                    CLAY_SCROLL({ .vertical = true }),
                    CLAY_LAYOUT({
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        .childGap = 16,
                        .padding = { 16, 16 },
                        .sizing = layoutExpand
                    })
                ) {
                }
            }
        }

        Clay_RenderCommandArray renderCommands = Clay_EndLayout();

        BeginDrawing();
        ClearBackground(BLACK);
        Clay_Raylib_Render(renderCommands);
        EndDrawing();
    }
}
