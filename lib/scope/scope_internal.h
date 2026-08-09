        // scope_internal.h

#pragma once

// =========================================================================================== IMPORT

#include "scope.h"

// =========================================================================================== IMPORT


/*

    Приватный заголовок реализации осциллографа.

    Нужен только для того, чтобы scope_logic.c и scope_gui.c видели общие
    внутренние функции друг друга. Приложение подключает исключительно scope.h.

    Такое разделение (публичный API + внутренний заголовок реализации) - обычная
    практика для больших C-модулей: публичный интерфейс остаётся маленьким и
    читаемым, а всё остальное не засоряет пространство имён пользователя модуля.

*/


// =========================================================================================== INIT and CLEAR helpers

// ===== Init (scope_logic.c) =====

void scope_main_settings_init(Scope* used_scope);


void scope_signal_buffer_init(Scope* used_scope);


void scope_running_signal_characteristics_init(Scope* used_scope);

void scope_measured_signal_characteristics_init(Scope* used_scope);

void scope_filter_init(Scope* used_scope);


void scope_peaks_ctx_init(Scope* used_scope);

void scope_wave_pattern_detector_former_init(Scope* used_scope);

void scope_wave_pattern_detector_init(Scope* used_scope);


// ===== Init (scope_gui.c) =====

void scope_gui_init(Scope* used_scope, SDL_Renderer* renderer);

void scope_screen_gui_init(Scope* used_scope);

// Освобождение шрифтов и текстур всех текстбоксов осциллографа
void scope_gui_destroy(Scope* used_scope);


// ===== Clear (scope_logic.c) =====

void scope_main_settings_clear(Scope* used_scope);


void scope_signal_buffer_clear(Scope* used_scope);


void scope_running_signal_characteristics_clear(Scope* used_scope);

void scope_measured_signal_characteristics_clear(Scope* used_scope);

void scope_filter_clear(Scope* used_scope);


void scope_peaks_ctx_clear(Scope* used_scope);

void scope_wave_pattern_detector_former_clear(Scope* used_scope);

void scope_wave_pattern_detector_clear(Scope* used_scope);


// ===== Clear (scope_gui.c) =====

void scope_screen_gui_clear(Scope* used_scope);

// =========================================================================================== INIT and CLEAR helpers


// =========================================================================================== SIGNAL ANALYSIS (scope_logic.c)

// ===== scope_fast_update() часть анализа =====

void scope_buffer_update(Scope* used_scope);            // Получение сигнала

void runtime_data_update(Scope* used_scope);            // Апдейт runtime-характеристик

// Обнаружение трендов и пиков для одного сэмпла буфера
void runtime_detect_trends(Scope* used_scope, int curr_idx, int prev_idx);

// Обнаружение полуволн (внутри обнаружения пиков)
void runtime_detect_halfwaves(Scope* used_scope, float current_value, double current_time, trend_type current_trend);

// Helper-функция для zero-cross детектора / halfwaves детектора
void halfwaves_detector_accumulation(Scope* used_scope, float current_value, double current_time);

// Helper-функция для zero-cross детектора / halfwaves детектора
void drop_zc_accumulation(Scope* used_scope, float current_value, double current_time);

// Helper-функция для заполнения буффера полуволн
void add_halfwave_in_buffer(Scope* used_scope, halfwave_data_ctx new_halfwave);


// ===== scope_slow_update() часть анализа =====

// Анализ буффера полуволн для получение паттерна и расчёта периода
void detect_pattern_and_period(Scope* used_scope);


// Определение основного паттерна (в виде количества полуволн)
int detect_pattern(Scope* used_scope);


// Определение периода по количеству полуволн в паттерне
float detect_period(Scope* used_scope, int pattern_steps);


// Определение степени различия между двумя полуволнами
float halfwave_distance(Scope* used_scope, const halfwave_data_ctx* halfwave_1, const halfwave_data_ctx* halfwave_2);


float normalized_difference(float a, float b);


// Сравнение 2 чисел
int compare_float(const void* a, const void* b);


// Анализ нескольких последних периодов для получения measured-характеристик
void measured_data_update(Scope* used_scope);


// Настройка фильтров по полученным measured-характеристикам
void renew_filter(Scope* used_scope);


// ===== Основные функции =====

// Анализ поступающего сигнала для получения runtime-характеристик
// и передачи данных о пиках в детектор полуволн - коллится в scope_fast_update()
void signal_fast_analysis(Scope* used_scope);

// Анализ поступающего сигнала для получения measured-характеристик
// и передачи данных о пиках в детектор полуволн - коллится в scope_slow_update()
void signal_slow_analysis(Scope* used_scope);

// =========================================================================================== SIGNAL ANALYSIS (scope_logic.c)


// =========================================================================================== SCOPE GUI (scope_gui.c)


void scope_gui_renew(Scope* used_scope);                                                    // Обновление графики осциллографа

void scope_screens_gui_renew_by_signal_data(Scope* used_scope);                             // Обновление текстбоксов по данным сигнала

void main_screen_renew(Scope* used_scope);                                                  // Обновление данных для рендера сигнала


void build_fixed_time_render(Scope* used_scope, signal_render_ctx* render_data);            // Развёртка с фикс. временем на деление

void build_scroll_render(Scope* used_scope, signal_render_ctx* render_data);                // Развёртка roll (самописец)

void build_fixed_period_render(Scope* used_scope, signal_render_ctx* render_data);          // Развёртка с фикс. кол-вом периодов


void draw_signal(Scope* used_scope, SDL_Renderer* renderer);                                // Функция рендеринга сигнала


void scope_display_animation(Scope* used_scope);                                            // Расчёт анимации мерцания дисплеев


// =========================================================================================== SCOPE GUI (scope_gui.c)


// =========================================================================================== SCOPE BUTTONS CALLBACKS (scope_gui.c)

void decrease_scope_value_scale(Button* btn);
void increase_scope_value_scale(Button* btn);


void decrease_scope_time_scale(Button* btn);
void increase_scope_time_scale(Button* btn);


void decrease_amplitude(Button* btn);
void increase_amplitude(Button* btn);


void decrease_frequency(Button* btn);
void increase_frequency(Button* btn);


void change_controlled_signal(Button* btn);

void change_scope_render_mode(Button* btn);

void play_controlled_signal(Button* btn);


void on_off_command(Button* btn);

// =========================================================================================== SCOPE BUTTONS CALLBACKS (scope_gui.c)
