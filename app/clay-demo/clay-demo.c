#include "clay/clay.h"
/******************************************************************************/
//#include "../../renderers/raylib/clay_renderer_raylib.c"
#include "raylib.h"
#include "raymath.h"
#include "stdint.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

#define CLAY_RECTANGLE_TO_RAYLIB_RECTANGLE(rectangle) (Rectangle) { .x = rectangle.x, .y = rectangle.y, .width = rectangle.width, .height = rectangle.height }
#define CLAY_COLOR_TO_RAYLIB_COLOR(color) (Color) { .r = (unsigned char)roundf(color.r), .g = (unsigned char)roundf(color.g), .b = (unsigned char)roundf(color.b), .a = (unsigned char)roundf(color.a) }

typedef struct
{
    uint32_t fontId;
    Font font;
} Raylib_Font;

Raylib_Font Raylib_fonts[10];
Camera Raylib_camera;

typedef enum
{
    CUSTOM_LAYOUT_ELEMENT_TYPE_3D_MODEL
} CustomLayoutElementType;

typedef struct
{
    Model model;
    float scale;
    Vector3 position;
    Matrix rotation;
} CustomLayoutElement_3DModel;

typedef struct
{
    CustomLayoutElementType type;
    union {
        CustomLayoutElement_3DModel model;
    };
} CustomLayoutElement;

// Get a ray trace from the screen position (i.e mouse) within a specific section of the screen
Ray GetScreenToWorldPointWithZDistance(Vector2 position, Camera camera, int screenWidth, int screenHeight, float zDistance)
{
    Ray ray = { 0 };

    // Calculate normalized device coordinates
    // NOTE: y value is negative
    float x = (2.0f*position.x)/(float)screenWidth - 1.0f;
    float y = 1.0f - (2.0f*position.y)/(float)screenHeight;
    float z = 1.0f;

    // Store values in a vector
    Vector3 deviceCoords = { x, y, z };

    // Calculate view matrix from camera look at
    Matrix matView = MatrixLookAt(camera.position, camera.target, camera.up);

    Matrix matProj = MatrixIdentity();

    if (camera.projection == CAMERA_PERSPECTIVE)
    {
        // Calculate projection matrix from perspective
        matProj = MatrixPerspective(camera.fovy*DEG2RAD, ((double)screenWidth/(double)screenHeight), 0.01f, zDistance);
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC)
    {
        double aspect = (double)screenWidth/(double)screenHeight;
        double top = camera.fovy/2.0;
        double right = top*aspect;

        // Calculate projection matrix from orthographic
        matProj = MatrixOrtho(-right, right, -top, top, 0.01, 1000.0);
    }

    // Unproject far/near points
    Vector3 nearPoint = Vector3Unproject((Vector3){ deviceCoords.x, deviceCoords.y, 0.0f }, matProj, matView);
    Vector3 farPoint = Vector3Unproject((Vector3){ deviceCoords.x, deviceCoords.y, 1.0f }, matProj, matView);

    // Calculate normalized direction vector
    Vector3 direction = Vector3Normalize(Vector3Subtract(farPoint, nearPoint));

    ray.position = farPoint;

    // Apply calculated vectors to ray
    ray.direction = direction;

    return ray;
}

uint32_t measureCalls = 0;

static inline Clay_Dimensions Raylib_MeasureText(Clay_String *text, Clay_TextElementConfig *config) {
    measureCalls++;
    // Measure string size for Font
    Clay_Dimensions textSize = { 0 };

    float maxTextWidth = 0.0f;
    float lineTextWidth = 0;

    float textHeight = config->fontSize;
    Font fontToUse = Raylib_fonts[config->fontId].font;
    float scaleFactor = config->fontSize/(float)fontToUse.baseSize;

    for (int i = 0; i < text->length; ++i)
    {
        if (text->chars[i] == '\n') {
            maxTextWidth = fmax(maxTextWidth, lineTextWidth);
            lineTextWidth = 0;
            continue;
        }
        int index = text->chars[i] - 32;
        if (fontToUse.glyphs[index].advanceX != 0) lineTextWidth += fontToUse.glyphs[index].advanceX;
        else lineTextWidth += (fontToUse.recs[index].width + fontToUse.glyphs[index].offsetX);
    }

    maxTextWidth = fmax(maxTextWidth, lineTextWidth);

    textSize.width = maxTextWidth * scaleFactor;
    textSize.height = textHeight;

    return textSize;
}

void Clay_Raylib_Initialize(int width, int height, const char *title, unsigned int flags) {
    SetConfigFlags(flags);
    InitWindow(width, height, title);
//    EnableEventWaiting();
}

void Clay_Raylib_Render(Clay_RenderCommandArray renderCommands)
{
    measureCalls = 0;
    for (int j = 0; j < renderCommands.length; j++)
    {
        Clay_RenderCommand *renderCommand = Clay_RenderCommandArray_Get(&renderCommands, j);
        Clay_BoundingBox boundingBox = renderCommand->boundingBox;
        switch (renderCommand->commandType)
        {
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                // Raylib uses standard C strings so isn't compatible with cheap slices, we need to clone the string to append null terminator
                Clay_String text = renderCommand->text;
                char *cloned = (char *)malloc(text.length + 1);
                memcpy(cloned, text.chars, text.length);
                cloned[text.length] = '\0';
                Font fontToUse = Raylib_fonts[renderCommand->config.textElementConfig->fontId].font;
                DrawTextEx(fontToUse, cloned, (Vector2){boundingBox.x, boundingBox.y}, (float)renderCommand->config.textElementConfig->fontSize, (float)renderCommand->config.textElementConfig->letterSpacing, CLAY_COLOR_TO_RAYLIB_COLOR(renderCommand->config.textElementConfig->textColor));
                free(cloned);
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
                Texture2D imageTexture = *(Texture2D *)renderCommand->config.imageElementConfig->imageData;
                DrawTextureEx(
                imageTexture,
                (Vector2){boundingBox.x, boundingBox.y},
                0,
                boundingBox.width / (float)imageTexture.width,
                WHITE);
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                BeginScissorMode((int)roundf(boundingBox.x), (int)roundf(boundingBox.y), (int)roundf(boundingBox.width), (int)roundf(boundingBox.height));
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                EndScissorMode();
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Clay_RectangleElementConfig *config = renderCommand->config.rectangleElementConfig;
                if (config->cornerRadius.topLeft > 0) {
                    float radius = (config->cornerRadius.topLeft * 2) / (float)((boundingBox.width > boundingBox.height) ? boundingBox.height : boundingBox.width);
                    DrawRectangleRounded((Rectangle) { boundingBox.x, boundingBox.y, boundingBox.width, boundingBox.height }, radius, 8, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
                } else {
                    DrawRectangle(boundingBox.x, boundingBox.y, boundingBox.width, boundingBox.height, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                Clay_BorderElementConfig *config = renderCommand->config.borderElementConfig;
                // Left border
                if (config->left.width > 0) {
                    DrawRectangle((int)roundf(boundingBox.x), (int)roundf(boundingBox.y + config->cornerRadius.topLeft), (int)config->left.width, (int)roundf(boundingBox.height - config->cornerRadius.topLeft - config->cornerRadius.bottomLeft), CLAY_COLOR_TO_RAYLIB_COLOR(config->left.color));
                }
                // Right border
                if (config->right.width > 0) {
                    DrawRectangle((int)roundf(boundingBox.x + boundingBox.width - config->right.width), (int)roundf(boundingBox.y + config->cornerRadius.topRight), (int)config->right.width, (int)roundf(boundingBox.height - config->cornerRadius.topRight - config->cornerRadius.bottomRight), CLAY_COLOR_TO_RAYLIB_COLOR(config->right.color));
                }
                // Top border
                if (config->top.width > 0) {
                    DrawRectangle((int)roundf(boundingBox.x + config->cornerRadius.topLeft), (int)roundf(boundingBox.y), (int)roundf(boundingBox.width - config->cornerRadius.topLeft - config->cornerRadius.topRight), (int)config->top.width, CLAY_COLOR_TO_RAYLIB_COLOR(config->top.color));
                }
                // Bottom border
                if (config->bottom.width > 0) {
                    DrawRectangle((int)roundf(boundingBox.x + config->cornerRadius.bottomLeft), (int)roundf(boundingBox.y + boundingBox.height - config->bottom.width), (int)roundf(boundingBox.width - config->cornerRadius.bottomLeft - config->cornerRadius.bottomRight), (int)config->bottom.width, CLAY_COLOR_TO_RAYLIB_COLOR(config->bottom.color));
                }
                if (config->cornerRadius.topLeft > 0) {
                    DrawRing((Vector2) { roundf(boundingBox.x + config->cornerRadius.topLeft), roundf(boundingBox.y + config->cornerRadius.topLeft) }, roundf(config->cornerRadius.topLeft - config->top.width), config->cornerRadius.topLeft, 180, 270, 10, CLAY_COLOR_TO_RAYLIB_COLOR(config->top.color));
                }
                if (config->cornerRadius.topRight > 0) {
                    DrawRing((Vector2) { roundf(boundingBox.x + boundingBox.width - config->cornerRadius.topRight), roundf(boundingBox.y + config->cornerRadius.topRight) }, roundf(config->cornerRadius.topRight - config->top.width), config->cornerRadius.topRight, 270, 360, 10, CLAY_COLOR_TO_RAYLIB_COLOR(config->top.color));
                }
                if (config->cornerRadius.bottomLeft > 0) {
                    DrawRing((Vector2) { roundf(boundingBox.x + config->cornerRadius.bottomLeft), roundf(boundingBox.y + boundingBox.height - config->cornerRadius.bottomLeft) }, roundf(config->cornerRadius.bottomLeft - config->top.width), config->cornerRadius.bottomLeft, 90, 180, 10, CLAY_COLOR_TO_RAYLIB_COLOR(config->bottom.color));
                }
                if (config->cornerRadius.bottomRight > 0) {
                    DrawRing((Vector2) { roundf(boundingBox.x + boundingBox.width - config->cornerRadius.bottomRight), roundf(boundingBox.y + boundingBox.height - config->cornerRadius.bottomRight) }, roundf(config->cornerRadius.bottomRight - config->bottom.width), config->cornerRadius.bottomRight, 0.1, 90, 10, CLAY_COLOR_TO_RAYLIB_COLOR(config->bottom.color));
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
                CustomLayoutElement *customElement = (CustomLayoutElement *)renderCommand->config.customElementConfig->customData;
                if (!customElement) continue;
                switch (customElement->type) {
                    case CUSTOM_LAYOUT_ELEMENT_TYPE_3D_MODEL: {
                        Clay_BoundingBox rootBox = renderCommands.internalArray[0].boundingBox;
                        float scaleValue = CLAY__MIN(CLAY__MIN(1, 768 / rootBox.height) * CLAY__MAX(1, rootBox.width / 1024), 1.5f);
                        Ray positionRay = GetScreenToWorldPointWithZDistance((Vector2) { renderCommand->boundingBox.x + renderCommand->boundingBox.width / 2, renderCommand->boundingBox.y + (renderCommand->boundingBox.height / 2) + 20 }, Raylib_camera, (int)roundf(rootBox.width), (int)roundf(rootBox.height), 140);
                        BeginMode3D(Raylib_camera);
                            DrawModel(customElement->model.model, positionRay.position, customElement->model.scale * scaleValue, WHITE);        // Draw 3d model with texture
                        EndMode3D();
                        break;
                    }
                    default: break;
                }
                break;
            }
            default: {
                printf("Error: unhandled render command.");
                exit(1);
            }
        }
    }
}
/******************************************************************************/

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
