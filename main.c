#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "raylib/raylib.h"
#include "raylib/clay_renderer_raylib.c"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define MOCK_VALUES_CNT 5
#define MAX_STRINGIFIED_NUMBER_LEN 32

const Clay_Color COLOR_LIGHT  = (Clay_Color) {224, 215, 210, 255};
const Clay_Color COLOR_RED    = (Clay_Color) {168,  66,  28, 255};
const Clay_Color COLOR_ORANGE = (Clay_Color) {225, 138,  50, 255};

typedef struct {
	Clay_TextElementConfig textConfig;
	float values[MOCK_VALUES_CNT];
	Clay_String valuesStr[MOCK_VALUES_CNT];
} Ctx;

// =====
// UI Rendering
// =====

void clayErrorHandler(Clay_ErrorData error) {
	printf("Clay Error: %s\n", error.errorText.chars);
	assert(0);
}

void buildGui(Ctx *ctx) {
	CLAY(CLAY_ID("Root"), {
		.layout = {
			.sizing = { 
				.width  = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_GROW(0),
			},
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
			.childAlignment  = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
			.childGap        = 50,
		},
		.backgroundColor = COLOR_LIGHT,
	}) {
		for (int i = 0; i < MOCK_VALUES_CNT; i++) {
			CLAY_TEXT(ctx->valuesStr[i], &ctx->textConfig);
		}
	}
}

// =====
// UART Signal Processing
// =====

int generateMockUartSignal(char *dst, size_t dstSize) {
	assert(dst != NULL || dstSize >= 32);

	float values[MOCK_VALUES_CNT];

	// Generate some pseudo‑changing values
	for (int i = 0; i < MOCK_VALUES_CNT; i++) {
		float jitter = ((float)rand() / RAND_MAX) * 0.1f; // [0..0.1]

		values[i] = (i + 10) + jitter;
	}

	// Serialize them 
	int written = snprintf(
		dst,
		dstSize,
		"%.2f,%.2f,%.2f,%.2f,%.2f",
		values[0], values[1], values[2], values[3], values[4]
	);

	assert(written > 0 && (size_t)written < dstSize);
}

void parseUartData(char *uartBuffer, Ctx *ctx) {
	assert(uartBuffer != NULL && ctx != NULL);

	for (int i = 0; i < MOCK_VALUES_CNT; i++) {
		float value;
		int charsConsumed = 0;

		if (sscanf(uartBuffer, "%f%n", &value, &charsConsumed) != 1) {
			printf("Failed to parse value #%d\n", i);
			assert(0);
		}

		ctx->values[i] = value;

		ctx->valuesStr[i] = (Clay_String){
			.chars = uartBuffer,
			.length = charsConsumed,
			.isStaticallyAllocated = true,
		};

		uartBuffer += charsConsumed;

		if (*uartBuffer == ',') {
			uartBuffer++;
		}
	}
}

void mockNewUartSignal(char *uartBuffer, size_t uartBufferSize, Ctx *ctx) {
	const double intervalSec = 0.3; // 300ms
	static double lastTime = 0;

	double now = GetTime();

	if ((now - lastTime) >= intervalSec) {
		lastTime = now;

		generateMockUartSignal(uartBuffer, uartBufferSize);
		parseUartData(uartBuffer, ctx);
	}
}

// =====
// Main entrypoint
// =====

int main() {
	srand(69420); // Seed

	Ctx ctx;
	char uartBuffer[256] = {0};

	// =====

	Clay_Raylib_Initialize(900, 600, "Battery Management System GUI", FLAG_WINDOW_RESIZABLE);

	uint64_t arenaSize = Clay_MinMemorySize();

	Clay_Arena clayMemoryArena = {
		.memory   = malloc(arenaSize),
		.capacity = arenaSize,
	};

	Clay_Initialize(
		clayMemoryArena,
		(Clay_Dimensions) {
			.width  = (float)GetScreenWidth(),
			.height = (float)GetScreenHeight()
		},
		(Clay_ErrorHandler) {
			.errorHandlerFunction = clayErrorHandler,
			.userData = NULL,
		}
	);

	SetTargetFPS(90);

	// =====

	Font fonts[1] = { LoadFont("assets/Roboto-Regular.ttf") };

	Clay_SetMeasureTextFunction(Raylib_MeasureText, fonts);

	ctx.textConfig = (Clay_TextElementConfig) {
		.fontSize      = 32,
		.textColor     = {0, 0, 0, 255},
		.textAlignment = CLAY_TEXT_ALIGN_CENTER,
	};

	while (!WindowShouldClose()) {
		// Update window dimensions & mouse state

		Clay_SetLayoutDimensions((Clay_Dimensions) {
			(float)GetScreenWidth(),
			(float)GetScreenHeight()
		});

		Vector2 mouse = GetMousePosition();
		Vector2 mouseScrollDelta = GetMouseWheelMoveV();

		Clay_SetPointerState(
			(Clay_Vector2) { mouse.x, mouse.y },
			IsMouseButtonDown(MOUSE_BUTTON_LEFT)
		);

		Clay_UpdateScrollContainers(
			true,
			(Clay_Vector2) { mouseScrollDelta.x, mouseScrollDelta.y },
			GetFrameTime()
		);

		// =====
		// Compute layout & render GUI

		// When live, this would be triggered by UART events instead of mocked every frame
		mockNewUartSignal(uartBuffer, sizeof(uartBuffer), &ctx);

		Clay_BeginLayout();
		buildGui(&ctx);

		Clay_RenderCommandArray renderCommands = Clay_EndLayout();

		BeginDrawing();	
		ClearBackground(WHITE);
		Clay_Raylib_Render(renderCommands, fonts);
		EndDrawing();
	}

	// =====

	free(clayMemoryArena.memory);

	return 0;
}
