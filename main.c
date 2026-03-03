// [Batter Management System GUI]
//
// This is a GUI for a battery management system for the battery of an electric car.
// The program receives data over UART from the battery pack. The battery consists of 
// multiple modules, where each module holds multiple battery cells. Each module has
// multiple temperature sensors at different points in the module.
// 
// The program is using Raylib for rendering and a small layout library called Clay
// for layout management.
//
// For testing purposes, we'll simulate receiving UART signals by generating
// random data every 300ms and parsing it as if it were received over UART.
// =====

#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "raylib/raylib.h"
#include "raylib/clay_renderer_raylib.c"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <float.h>

#define MODULES_CNT                4
#define PCB_CNT                    4
#define CELL_CNT_PER_MODULE        40
#define TEMP_SENSOR_CNT_PER_MODULE 12

#define VALUES_TO_GENERATE_CNT (                 \
    (MODULES_CNT * CELL_CNT_PER_MODULE) +        \
    (MODULES_CNT * TEMP_SENSOR_CNT_PER_MODULE) + \
    (PCB_CNT) +                                  \
    14                                           \
)                                                // 14x non-array fields in BmsState struct

// This implementation assumes that no stringified float will take up more than 32 characters
#define MAX_STRINGIFIED_FLOAT_LEN 10

typedef struct {
	float asFloat;
	Clay_String asString;
} FloatAndStr;

typedef struct {
	FloatAndStr cellVoltages[MODULES_CNT * CELL_CNT_PER_MODULE]; //     Voltages of all cells across all modules
	FloatAndStr cellTemps[MODULES_CNT * TEMP_SENSOR_CNT_PER_MODULE]; // Temps of all sensors across all modules
	FloatAndStr pcbTemps[PCB_CNT];
	FloatAndStr batteryAvgTemp;
	FloatAndStr humidity;
	FloatAndStr minTemp;
	FloatAndStr maxTemp;
	FloatAndStr avgTemp;
	FloatAndStr voltageLow;
	FloatAndStr voltagePack;
	FloatAndStr currentLow;
	FloatAndStr currentPack;
	FloatAndStr minBMSTemp;
	FloatAndStr maxBMSTemp;
	FloatAndStr minCellVolt;
	FloatAndStr maxCellVolt;
	FloatAndStr stateOfCharge;
} BmsState;

const Clay_Color COLOR_LIGHT  = (Clay_Color) {224, 215, 210, 255};
const Clay_Color COLOR_RED    = (Clay_Color) {168,  66,  28, 255};
const Clay_Color COLOR_ORANGE = (Clay_Color) {225, 138,  50, 255};

typedef struct {
	Clay_TextElementConfig textConfig;
	BmsState state;
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
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = COLOR_LIGHT,
	}) {
		CLAY_AUTO_ID({
			.layout = {
				.sizing = { 
					.width  = CLAY_SIZING_GROW(0),
					.height = CLAY_SIZING_GROW(0),
				},
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
				.childAlignment  = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
				.childGap        = 25,
			},
		}) {
			for (int i = 0; i < PCB_CNT; i++) {
				CLAY_TEXT(ctx->state.pcbTemps[i].asString, &ctx->textConfig);
			}
		}
	}
}

// =====
// UART Signal Processing
// =====

static float simulateJitter(float max) {
	return ((float)rand() / RAND_MAX) * max;
}

void generateMockUartSignal(char *dst, int dstSize) {
	float values[VALUES_TO_GENERATE_CNT];
	int idx = 0;

	for (int i = 0; i < MODULES_CNT * CELL_CNT_PER_MODULE; i++) {
		values[idx++] = 3.7 + simulateJitter(0.5); // Cell voltages
	}

	for (int i = 0; i < MODULES_CNT * TEMP_SENSOR_CNT_PER_MODULE; i++) {
		values[idx++] = 20 + simulateJitter(20); // Cell temps
	}

	for (int i = 0; i < PCB_CNT; i++) {
		values[idx++] = 25 + simulateJitter(20); // pcbTemps
	}

	values[idx++] = 30    + simulateJitter(5);   // batteryAvgTemp
	values[idx++] = 20    + simulateJitter(60);  // humidity
	values[idx++] = 20    + simulateJitter(5);   // minTemp
	values[idx++] = 40    + simulateJitter(10);  // maxTemp
	values[idx++] = 30    + simulateJitter(5);   // avgTemp
	values[idx++] = 11.5  + simulateJitter(1.5); // voltageLow
	values[idx++] = 400   + simulateJitter(20);  // voltagePack
	values[idx++] = -5    + simulateJitter(10);  // currentLow
	values[idx++] = -100  + simulateJitter(200); // currentPack
	values[idx++] = 45    + simulateJitter(10);  // maxBMSTemp
	values[idx++] = 25    + simulateJitter(5);   // minBMSTemp
	values[idx++] = 4.15  + simulateJitter(0.1); // maxCellVolt
	values[idx++] = 3.50  + simulateJitter(0.1); // minCellVolt
	values[idx++] = 0     + simulateJitter(100); // stateOfCharge

	assert(idx == VALUES_TO_GENERATE_CNT);

	// =====

	int written = 0;

	for (int i = 0; i < idx; i++) {
		int n = snprintf(
			dst + written,
			dstSize - written,
			(i == idx - 1) ? "%.2f" : "%.2f,",
			values[i]
		);

		assert(n >= 0);

		written += n;
	}
}

void parseUartData(char *uartBuffer, Ctx *ctx) {
	const int CELLS_TOTAL      = MODULES_CNT * CELL_CNT_PER_MODULE;
	const int TEMPS_TOTAL      = MODULES_CNT * TEMP_SENSOR_CNT_PER_MODULE;
	const int BASE_SCALARS_IDX = CELLS_TOTAL + TEMPS_TOTAL + PCB_CNT;

	for (int i = 0; i < VALUES_TO_GENERATE_CNT; i++) {
		// Pointer to the destination field in the BmsState struct where the next parsed value will be stored
		FloatAndStr *dst;

		if (i < CELLS_TOTAL) {                                         // cellVoltages
			dst = &ctx->state.cellVoltages[i];
		} else if (i < CELLS_TOTAL + TEMPS_TOTAL) {                    // cellTemps
			dst = &ctx->state.cellTemps[i - CELLS_TOTAL];
		} else if (i < CELLS_TOTAL + TEMPS_TOTAL + PCB_CNT) {          // pcbTemps
			dst = &ctx->state.pcbTemps[i - CELLS_TOTAL - TEMPS_TOTAL];
		} else {                                                       // Non-array fields
			switch (i - BASE_SCALARS_IDX) {
				case 0:  dst = &ctx->state.batteryAvgTemp; break;
				case 1:  dst = &ctx->state.humidity;       break;
				case 2:  dst = &ctx->state.minTemp;        break;
				case 3:  dst = &ctx->state.maxTemp;        break;
				case 4:  dst = &ctx->state.avgTemp;        break;
				case 5:  dst = &ctx->state.voltageLow;     break;
				case 6:  dst = &ctx->state.voltagePack;    break;
				case 7:  dst = &ctx->state.currentLow;     break;
				case 8:  dst = &ctx->state.currentPack;    break;
				case 9:  dst = &ctx->state.maxBMSTemp;     break;
				case 10: dst = &ctx->state.minBMSTemp;     break;
				case 11: dst = &ctx->state.maxCellVolt;    break;
				case 12: dst = &ctx->state.minCellVolt;    break;
				case 13: dst = &ctx->state.stateOfCharge;  break;
				default: assert(0);
			}
		}


		// =====

		dst->asString.chars = uartBuffer;

		if (sscanf(uartBuffer, "%f%n", &dst->asFloat, &dst->asString.length) != 1) {
			printf("Failed to parse value #%d\n", i);
			assert(0);
		}

		uartBuffer += dst->asString.length;

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

void printBmsState(BmsState *state) {
	printf("  batteryAvgTemp: %.2f\n", state->batteryAvgTemp.asFloat);
	printf("        humidity: %.2f\n", state->humidity.asFloat);
	printf("         minTemp: %.2f\n", state->minTemp.asFloat);
	printf("         maxTemp: %.2f\n", state->maxTemp.asFloat);
	printf("         avgTemp: %.2f\n", state->avgTemp.asFloat);
	printf("      voltageLow: %.2f\n", state->voltageLow.asFloat);
	printf("     voltagePack: %.2f\n", state->voltagePack.asFloat);
	printf("      currentLow: %.2f\n", state->currentLow.asFloat);
	printf("     currentPack: %.2f\n", state->currentPack.asFloat);
	printf("      maxBMSTemp: %.2f\n", state->maxBMSTemp.asFloat);
	printf("      minBMSTemp: %.2f\n", state->minBMSTemp.asFloat);
	printf("     maxCellVolt: %.2f\n", state->maxCellVolt.asFloat);
	printf("     minCellVolt: %.2f\n", state->minCellVolt.asFloat);
	printf("   stateOfCharge: %.2f\n", state->stateOfCharge.asFloat);

	for (int i = 0; i < PCB_CNT; i++) {
		printf("    pcbTemps[%d]: %.2f\n", i, state->pcbTemps[i].asFloat);
	}

	for (int i = 0; i < MODULES_CNT * CELL_CNT_PER_MODULE; i++) {
		printf("cellVoltages[%d]: %.2f\n", i, state->cellVoltages[i].asFloat);
	}

	for (int i = 0; i < MODULES_CNT * TEMP_SENSOR_CNT_PER_MODULE; i++) {
		printf("   cellTemps[%d]: %.2f\n", i, state->cellTemps[i].asFloat);
	}

	printf("\n");
}

void *safeMalloc(size_t size) {
	void *ptr = malloc(size);

	assert(ptr != NULL);

	return ptr;
}

int main() {
	srand(69420); // Seed

	Clay_Raylib_Initialize(900, 600, "Battery Management System GUI", FLAG_WINDOW_RESIZABLE);

	uint64_t arenaSize = Clay_MinMemorySize();

	Clay_Arena clayMemoryArena = {
		.memory   = safeMalloc(arenaSize),
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

	Clay_String uartBuffer = (Clay_String){
		.chars  = safeMalloc(VALUES_TO_GENERATE_CNT * MAX_STRINGIFIED_FLOAT_LEN),
		.length = VALUES_TO_GENERATE_CNT * MAX_STRINGIFIED_FLOAT_LEN,
	};

	Ctx ctx = {0};

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

		// When live, this would come form the UART stream
		mockNewUartSignal(uartBuffer.chars, uartBuffer.length, &ctx);

		Clay_BeginLayout();
		buildGui(&ctx);

		Clay_RenderCommandArray renderCommands = Clay_EndLayout();

		BeginDrawing();	
		ClearBackground(WHITE);
		Clay_Raylib_Render(renderCommands, fonts);
		EndDrawing();
	}

	// =====

	free(uartBuffer.chars);
	free(clayMemoryArena.memory);

	return 0;
}
