#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "raylib/raylib.h"
#include "raylib/clay_renderer_raylib.c"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define MOCK_VALUES_CNT 5
#define MAX_STRINGIFIED_NUMBER_LEN 32

typedef struct {
	Clay_TextElementConfig textConfig;
	float values[MOCK_VALUES_CNT];
	Clay_String valuesStr[MOCK_VALUES_CNT];
} Ctx;

void clayErrorHandler(Clay_ErrorData error) {
	printf("Clay Error: %s\n", error.errorText.chars);
	assert(0);
}

void renderClayUi(Ctx *ctx) {
	CLAY(CLAY_ID("root"), {
		.layout = {
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
			.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
			.childGap = 50,
		}
	}) {
		for (int i = 0; i < MOCK_VALUES_CNT; i++) {\
			CLAY_TEXT(ctx->valuesStr[i], &ctx->textConfig);
		}
	}
}

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

void updateState(char *uartBuffer, size_t uartBufferSize, Ctx *ctx) {
	const double interval = 0.05;
	static double lastTime = 0.0;

	double now = GetTime();

	if ((now - lastTime) >= interval) {
		lastTime = now;

		generateMockUartSignal(uartBuffer, uartBufferSize);
		parseUartData(uartBuffer, ctx);
	}
}

int main() {
	srand(69420); // Seed

	Ctx ctx;
	char uartBuffer[256] = {0};

	// =====

	const int SCREEN_WIDTH = 900;
	const int SCREEN_HEIGHT = 600;

	Clay_Raylib_Initialize(
		SCREEN_WIDTH,
		SCREEN_HEIGHT,
		"Battery Management System GUI",
		0 // Flags
	);

	uint64_t arenaSize = Clay_MinMemorySize();

	Clay_Initialize(
		(Clay_Arena) { 
			.memory = malloc(arenaSize),
			.capacity = arenaSize,
		},
		(Clay_Dimensions) {
			.width = (float)SCREEN_WIDTH,
			.height = (float)SCREEN_HEIGHT
		},
		(Clay_ErrorHandler){
			.errorHandlerFunction = clayErrorHandler,
			.userData = NULL,
		}
	);

	SetTargetFPS(90);

	// =====

	Font fonts[1] = { GetFontDefault() };

	Clay_SetMeasureTextFunction(Raylib_MeasureText, fonts);

	ctx.textConfig = (Clay_TextElementConfig) {
		.fontSize = 32,
		.textColor = { 0, 0, 0, 255 },
		.textAlignment = CLAY_TEXT_ALIGN_CENTER,
	};

	while (!WindowShouldClose()) {		
		updateState(uartBuffer, sizeof(uartBuffer), &ctx);
		
		Clay_BeginLayout();

		renderClayUi(&ctx);

		Clay_RenderCommandArray renderCommands = Clay_EndLayout();

		BeginDrawing();	
		ClearBackground(RAYWHITE);
		Clay_Raylib_Render(renderCommands, fonts);
		EndDrawing();
	}

	// =====

	return 0;
}
