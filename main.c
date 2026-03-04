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

#define MODULE_CNT                 4
#define PCB_CNT                    4
#define CELL_CNT_PER_MODULE        40
#define TEMP_SENSOR_CNT_PER_MODULE 12

#define VALUES_TO_GENERATE_CNT (                 \
	(MODULE_CNT * CELL_CNT_PER_MODULE) +         \
	(MODULE_CNT * TEMP_SENSOR_CNT_PER_MODULE) +  \
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
	Clay_String label;
	Clay_String value;
} TableRow;

typedef struct {
	Clay_String title;
	TableRow *rows;
	int rowCount;
} Table;

typedef struct {
	FloatAndStr cellVoltages[MODULE_CNT * CELL_CNT_PER_MODULE];     // Voltages of all cells across all modules
	FloatAndStr cellTemps[MODULE_CNT * TEMP_SENSOR_CNT_PER_MODULE]; // Temps of all sensors across all modules
	FloatAndStr pcbTemps[PCB_CNT];                                  // DONE
	FloatAndStr batteryAvgTemp;                                     // DONE
	FloatAndStr humidity;                                           // DONE
	FloatAndStr minTemp;                                            // DONE
	FloatAndStr maxTemp;                                            // DONE
	FloatAndStr avgTemp;                                            // DONE
	FloatAndStr voltageLow;                                         // 
	FloatAndStr voltagePack;                                        // 
	FloatAndStr currentLow;                                         // 
	FloatAndStr currentPack;                                        // 
	FloatAndStr minBMSTemp;                                         // 
	FloatAndStr maxBMSTemp;                                         // 
	FloatAndStr minCellVolt;                                        // 
	FloatAndStr maxCellVolt;                                        // 
	FloatAndStr stateOfCharge;                                      // 
} BmsState;

typedef struct {
	struct {
		Clay_TextElementConfig text;
		Clay_TextElementConfig boldText;
		Clay_TextElementConfig heading1;
		Clay_TextElementConfig heading2;
		Clay_TextElementConfig heading3;
	} textConfigs;
	BmsState state;
} Ctx;

const Clay_Color COLOR_RED        = {191,  50,   8, 255};
const Clay_Color COLOR_GREEN      = {  0, 204,  58, 255};
const Clay_Color COLOR_BLUE       = {  0,  58, 204, 255};
const Clay_Color COLOR_BLACK_BG   = {  9,   9,   9, 255};
const Clay_Color COLOR_GRAY_BG    = { 25,  26,  28, 255};
const Clay_Color COLOR_BORDER     = { 33,  34,  36, 255};
const Clay_Color COLOR_TEXT_GRAY  = { 58,  58,  60, 255};
const Clay_Color COLOR_TEXT_WHITE = {173, 174, 176, 255};

const Clay_BorderElementConfig BORDER       = { COLOR_BORDER, CLAY_BORDER_OUTSIDE(2) };
const Clay_BorderElementConfig BORDER_RED   = { COLOR_RED,    CLAY_BORDER_OUTSIDE(2) };
const Clay_BorderElementConfig BORDER_GREEN = { COLOR_GREEN,  CLAY_BORDER_OUTSIDE(2) };
const Clay_BorderElementConfig BORDER_BLUE  = { COLOR_BLUE,   CLAY_BORDER_OUTSIDE(2) };

// =====
// UI Rendering
// =====

static float clamp(float value, float min, float max) {
	return (value < min) ? min : (value > max) ? max : value;
}

Clay_Color lerpBetween2Colors(float n, float min, float max) {
	n = clamp(n, min, max);

	float t = clamp((n - min) / (max - min), 0, 1);
	float r = 255 * t;
	float g = 255 * (1 - t);

	return (Clay_Color){r, g, 0, 255};
}

void buildModuleInfoCard(int moduleIdx, Ctx *ctx) {
	Clay_String title = CLAY_STRING("Module X");

	switch (moduleIdx) {
	case 0: title.chars = "Module 1"; break;
	case 1: title.chars = "Module 2"; break;
	case 2: title.chars = "Module 3"; break;
	case 3: title.chars = "Module 4"; break;
	default: assert(0); break;
	}

	// =====

	CLAY_AUTO_ID({
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0) },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
			.padding         = CLAY_PADDING_ALL(8),
			.childAlignment  = { CLAY_ALIGN_X_CENTER },
			.childGap        = 8,
		},
		.cornerRadius    = CLAY_CORNER_RADIUS(8),
		.backgroundColor = COLOR_GRAY_BG,
		.border          = BORDER,
	}) {
		CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_GROW(0) }, .childAlignment = { CLAY_ALIGN_X_CENTER } } }) {
			CLAY_TEXT(title, &ctx->textConfigs.heading2);
		}

		CLAY_AUTO_ID({ .layout = { .childGap = 8 } }) {
			const int TABLE_1_ROW_CNT = 6;
			const int TABLE_1_COL_CNT = CELL_CNT_PER_MODULE / TABLE_1_ROW_CNT;
			const int TABLE_2_ROW_CNT = 6;
			const int TABLE_2_COL_CNT = TEMP_SENSOR_CNT_PER_MODULE / TABLE_2_ROW_CNT;
			const int ROW_GAP         = 4;
			const int CELL_SIZE       = 35;

			CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = ROW_GAP } }) {
				CLAY_TEXT(CLAY_STRING("Voltage (V)"), &ctx->textConfigs.heading3);

				for (int r = 0; r < TABLE_1_ROW_CNT; r++) {
					CLAY_AUTO_ID({
						.layout = { .childGap = 4 }
					}) {
						for (int c = 0; c < CELL_CNT_PER_MODULE / TABLE_1_ROW_CNT; c++) {
							int idx = moduleIdx * CELL_CNT_PER_MODULE + r * (CELL_CNT_PER_MODULE / TABLE_1_ROW_CNT) + c;

							FloatAndStr cellVoltage = ctx->state.cellVoltages[idx];
							Clay_Color bg = lerpBetween2Colors(cellVoltage.asFloat, ctx->state.minCellVolt.asFloat, ctx->state.maxCellVolt.asFloat);

							CLAY_AUTO_ID({
								.layout = {
									.sizing         = { CLAY_SIZING_FIXED(CELL_SIZE), CLAY_SIZING_FIXED(CELL_SIZE) },
									.childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
								},
								.backgroundColor = bg,
							}) {
								CLAY_TEXT(
									cellVoltage.asString,
									CLAY_TEXT_CONFIG({ .fontSize = 14, .textColor = {0, 0, 0, 255} })
								);
							}
						}
					}
				}
			}

			// =====

			CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = ROW_GAP } }) {
				CLAY_TEXT(CLAY_STRING("Temp (C)"), &ctx->textConfigs.heading3);

				for (int r = 0; r < TABLE_2_ROW_CNT; r++) {
					CLAY_AUTO_ID({
						.layout = { .childGap = 4 }
					}) {
						for (int c = 0; c < TEMP_SENSOR_CNT_PER_MODULE / TABLE_2_ROW_CNT; c++) {
							int idx = moduleIdx * TEMP_SENSOR_CNT_PER_MODULE + r * (TEMP_SENSOR_CNT_PER_MODULE / TABLE_2_ROW_CNT) + c;

							FloatAndStr cellTemp = ctx->state.cellTemps[idx];
							Clay_Color bg = lerpBetween2Colors(cellTemp.asFloat, ctx->state.minTemp.asFloat, ctx->state.maxTemp.asFloat);

							CLAY_AUTO_ID({
								.layout = {
									.sizing         = { CLAY_SIZING_FIXED(CELL_SIZE), CLAY_SIZING_FIXED(CELL_SIZE) },
									.childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
								},
								.backgroundColor = bg,
							}) {
								CLAY_TEXT(
									cellTemp.asString,
									CLAY_TEXT_CONFIG({ .fontSize = 14, .textColor = {0, 0, 0, 255} })
								);
							}
						}
					}
				}
			}
		}
	}

}

void buildTableCard(Table *table, Ctx *ctx) {
	CLAY_AUTO_ID({
		.layout = {
			.sizing          = { CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0) },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
			.padding         = CLAY_PADDING_ALL(8),
			.childGap        = 6,
		},
		.cornerRadius    = CLAY_CORNER_RADIUS(8),
		.backgroundColor = COLOR_GRAY_BG,
		.border          = BORDER,
	}) {
		// Header row
		CLAY_AUTO_ID({ .layout = { .padding = { 0, 0, 0, 8 } } }) {
			CLAY_TEXT(table->title, &ctx->textConfigs.heading2);
		}

		// Data rows
		for (int i = 0; i < table->rowCount; i++) {
			TableRow row = table->rows[i];

			CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_GROW(0) }, .childGap = 4 } }) {
				CLAY_TEXT(row.label, &ctx->textConfigs.text);

				CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_GROW(0) } } });

				CLAY_TEXT(row.value, &ctx->textConfigs.text);
			}
		}
	}
}

void buildGui(Ctx *ctx) {
	TableRow pcbTempsTableRows[PCB_CNT] = {
		(TableRow) { .label = CLAY_STRING("PCB 1"), .value = ctx->state.pcbTemps[0].asString },
		(TableRow) { .label = CLAY_STRING("PCB 2"), .value = ctx->state.pcbTemps[1].asString },
		(TableRow) { .label = CLAY_STRING("PCB 3"), .value = ctx->state.pcbTemps[2].asString },
		(TableRow) { .label = CLAY_STRING("PCB 4"), .value = ctx->state.pcbTemps[3].asString },
	};

	Table pcbTempsTable = {
		.title    = CLAY_STRING("PCB Temps (C)"),
		.rows     = pcbTempsTableRows,
		.rowCount = PCB_CNT,
	};

	// =====

	TableRow ambientInfoTableRows[4] = {
		(TableRow) { .label = CLAY_STRING("Min. Temp (C)"), .value = ctx->state.minTemp.asString  },
		(TableRow) { .label = CLAY_STRING("Max. Temp (C)"), .value = ctx->state.maxTemp.asString  },
		(TableRow) { .label = CLAY_STRING("Avg. Temp (C)"), .value = ctx->state.avgTemp.asString  },
		(TableRow) { .label = CLAY_STRING("Humidity"),      .value = ctx->state.humidity.asString },
	};

	Table ambientInfoTable = {
		.title    = CLAY_STRING("Ambient Info"),
		.rows     = ambientInfoTableRows,
		.rowCount = 4,
	};

	// =====

	TableRow bmsTempsTableRows[2] = {
		(TableRow) { .label = CLAY_STRING("Min. BMS Temp (C)"), .value = ctx->state.minBMSTemp.asString },
		(TableRow) { .label = CLAY_STRING("Max. BMS Temp (C)"), .value = ctx->state.maxBMSTemp.asString },
	};

	Table bmsTempsTable = {
		.title    = CLAY_STRING("BMS PCB Temps"),
		.rows     = bmsTempsTableRows,
		.rowCount = 2,
	};

	// =====

	TableRow cellVoltageTableRows[3] = {
		(TableRow) { .label = CLAY_STRING("Min. Cell Volt (V)"), .value = ctx->state.minCellVolt.asString },
		(TableRow) { .label = CLAY_STRING("Max. Cell Volt (V)"), .value = ctx->state.maxCellVolt.asString },
		(TableRow) { .label = CLAY_STRING("Pack Voltage (V)"),   .value = ctx->state.voltagePack.asString },
	};

	Table cellVoltageTable = {
		.title    = CLAY_STRING("Cell Voltages"),
		.rows     = cellVoltageTableRows,
		.rowCount = 3,
	};

	// =====

	TableRow packCurrentTableRows[3] = {
		(TableRow) { .label = CLAY_STRING("LV Voltage (V)"),   .value = ctx->state.voltageLow.asString  },
		(TableRow) { .label = CLAY_STRING("LV Current (A)"),   .value = ctx->state.currentLow.asString  },
		(TableRow) { .label = CLAY_STRING("Pack Current (A)"), .value = ctx->state.currentPack.asString },
	};

	Table packCurrentTable = {
		.title    = CLAY_STRING("Voltages & Currents"),
		.rows     = packCurrentTableRows,
		.rowCount = 3,
	};

	// =====

	CLAY(CLAY_ID("Root"), {
		.layout = {
			.sizing          = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
			.childAlignment  = { CLAY_ALIGN_X_CENTER },
		},
		.clip = {
			.vertical    = true,
			.horizontal  = true,
			.childOffset = Clay_GetScrollOffset(),
		},
		.backgroundColor = COLOR_BLACK_BG,
	}) {
		CLAY_AUTO_ID({
			.layout = {
				.sizing          = { CLAY_SIZING_GROW(0, 1380) },
				.layoutDirection = CLAY_TOP_TO_BOTTOM,
				.padding         = CLAY_PADDING_ALL(16),
				.childGap        = 8,
			}
		}) {
			CLAY_AUTO_ID({
				.layout = {
					.sizing         = { CLAY_SIZING_GROW(0) },
					.padding        = CLAY_PADDING_ALL(8),
					.childAlignment = { CLAY_ALIGN_X_CENTER },
				},
				.cornerRadius    = CLAY_CORNER_RADIUS(8),
				.backgroundColor = COLOR_GRAY_BG,
				.border          = BORDER,
			}) {
				CLAY_TEXT(CLAY_STRING("Battery Management System"), &ctx->textConfigs.heading1);
			}

			CLAY_AUTO_ID({
				.layout = {
					.sizing         = { CLAY_SIZING_GROW(0) },
					.childAlignment = { CLAY_ALIGN_X_CENTER },
					.padding        = { 0, 0, 16, 0 }, // Top margin
					.childGap       = 8,
				}
			}) {
				for (int i = 0; i < MODULE_CNT; i++) {
					buildModuleInfoCard(i, ctx);
				}
			}

			CLAY_AUTO_ID({
				.layout = {
					.sizing   = { CLAY_SIZING_GROW(0) },
					.childGap = 8,
				}
			}) {
				buildTableCard(&pcbTempsTable, ctx);
				buildTableCard(&ambientInfoTable, ctx);
				buildTableCard(&bmsTempsTable, ctx);
				buildTableCard(&cellVoltageTable, ctx);
				buildTableCard(&packCurrentTable, ctx);

				CLAY_AUTO_ID({
					.layout = {
						.sizing  = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
						.padding = CLAY_PADDING_ALL(8),
					},
					.cornerRadius    = CLAY_CORNER_RADIUS(8),
					.backgroundColor = COLOR_GRAY_BG,
					.border          = BORDER,
				}) {}
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

	for (int i = 0; i < MODULE_CNT * CELL_CNT_PER_MODULE; i++) {
		values[idx++] = 3.7 + simulateJitter(0.5); // Cell voltages
	}

	for (int i = 0; i < MODULE_CNT * TEMP_SENSOR_CNT_PER_MODULE; i++) {
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
	const int CELLS_TOTAL      = MODULE_CNT * CELL_CNT_PER_MODULE;
	const int TEMPS_TOTAL      = MODULE_CNT * TEMP_SENSOR_CNT_PER_MODULE;
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
	const double intervalSec = 2; // 2s
	static double lastTime = 0;

	double now = GetTime();

	if (!lastTime || (now - lastTime) >= intervalSec) {
		lastTime = now;

		generateMockUartSignal(uartBuffer, uartBufferSize);
		parseUartData(uartBuffer, ctx);
	}
}

// =====

static void *safeMalloc(size_t size) {
	void *ptr = malloc(size);

	assert(ptr != NULL);

	return ptr;
}

void clayErrorHandler(Clay_ErrorData error) {
	printf("Clay Error: %s\n", error.errorText.chars);
	assert(0);
}

int main() {
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

	// Clay_SetDebugModeEnabled(true);
	SetTargetFPS(90);

	// =====

	// Note: `fontId`-s are index references to this array
	Font fonts[] = {
		LoadFont("assets/Roboto-Regular.ttf"),
		LoadFont("assets/Roboto-SemiBold.ttf"),
		LoadFont("assets/Roboto-Bold.ttf"),
	};

	Clay_SetMeasureTextFunction(Raylib_MeasureText, fonts);

	Clay_String uartBuffer = (Clay_String){
		.chars  = safeMalloc(VALUES_TO_GENERATE_CNT * MAX_STRINGIFIED_FLOAT_LEN),
		.length = VALUES_TO_GENERATE_CNT * MAX_STRINGIFIED_FLOAT_LEN,
	};

	Ctx ctx = {0};

	ctx.textConfigs.text = (Clay_TextElementConfig) {
		.fontId        = 0,
		.fontSize      = 18,
		.textColor     = COLOR_TEXT_WHITE,
		.textAlignment = CLAY_TEXT_ALIGN_CENTER,
	};

	ctx.textConfigs.heading1 = (Clay_TextElementConfig) {
		.fontId        = 2,
		.fontSize      = 36,
		.textColor     = COLOR_TEXT_WHITE,
		.textAlignment = CLAY_TEXT_ALIGN_CENTER,
	};

	ctx.textConfigs.heading2 = (Clay_TextElementConfig) {
		.fontId        = 1,
		.fontSize      = 24,
		.textColor     = COLOR_TEXT_WHITE,
		.textAlignment = CLAY_TEXT_ALIGN_CENTER,
	};

	ctx.textConfigs.heading3 = (Clay_TextElementConfig) {
		.fontId        = 2,
		.fontSize      = 20,
		.textColor     = COLOR_TEXT_WHITE,
		.textAlignment = CLAY_TEXT_ALIGN_CENTER,
		.wrapMode      = CLAY_TEXT_WRAP_NONE,
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
