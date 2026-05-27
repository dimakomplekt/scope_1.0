# Если что-то падает — PowerShell сразу останавливает скрипт
$ErrorActionPreference = "Stop"

# =========================
# КОМПИЛЯТОР
# =========================
# gcc = компилятор C/C++ из MinGW
$CC = "gcc"

# =========================
# ИСХОДНИКИ ПРОЕКТА
# =========================
# Все .c файлы, которые будут собраны в один .exe
$SRC = @(
    "src/main.c",
    "src/app.c",
    "lib/scope/scope.c",
    "lib/sin_generator/sin_generator.c",
    "lib/app_timer/app_timer.c",
    "lib/SDL2/my_sdl_draw/my_sdl_draw.c",
    "lib/SDL2/global_inputs/global_inputs.c",
    "lib/SDL2/UI_elements/my_sdl_button/my_sdl_button.c"
)

# =========================
# ПАПКА СБОРКИ
# =========================
# сюда будет собран exe
$BUILD_DIR = "build"
$OUT = "$BUILD_DIR/app.exe"

# =========================
# SDL2 БИБЛИОТЕКИ
# =========================
# Это "корни" SDL и SDL_ttf
# внутри них есть:
#   include/SDL.h
#   include/SDL2/SDL.h
#   lib/...
#   bin/...
$SDL = "./lib/SDL2/SDL2-2.30.8/x86_64-w64-mingw32"
$TTF = "./lib/SDL2/SDL2_ttf-2.24.0/x86_64-w64-mingw32"

# =========================
# СОЗДАЁМ ПАПКУ build
# =========================
# если её нет — создаём
if (!(Test-Path $BUILD_DIR)) {
    New-Item -ItemType Directory $BUILD_DIR | Out-Null
}

Write-Host "Building..." -ForegroundColor Cyan

# =========================
# АРГУМЕНТЫ GCC
# =========================
# сюда мы собираем команду компиляции по частям
$ARGS = @()

# -------------------------
# 1. исходники
# -------------------------
# какие .c файлы компилировать
$ARGS += $SRC

# -------------------------
# 2. INCLUDE PATHS (где искать .h файлы)
# -------------------------
# gcc ищет заголовки через -I

# основной SDL include
$ARGS += "-I$SDL/include"

# иногда SDL лежит глубже как SDL2/SDL.h
# поэтому добавляем второй путь
$ARGS += "-I$SDL/include/SDL2"

# аналогично для SDL_ttf
$ARGS += "-I$TTF/include"
$ARGS += "-I$TTF/include/SDL2"

# -------------------------
# 3. LIB PATHS (где искать .a / .lib файлы)
# -------------------------
# это папки с бинарными библиотеками SDL
$ARGS += "-L$SDL/lib"
$ARGS += "-L$TTF/lib"

# -------------------------
# 4. БИБЛИОТЕКИ (что линковать)
# -------------------------
# -l = подключить библиотеку

# базовый слой MinGW
$ARGS += "-lmingw32"

# SDL main-обёртка (подменяет main на SDL_main)
$ARGS += "-lSDL2main"

# сама SDL2
$ARGS += "-lSDL2"

# расширение SDL для работы со шрифтами
$ARGS += "-lSDL2_ttf"

# -------------------------
# 5. ФЛАГ SDL MAIN FIX
# -------------------------
# отключает конфликт SDL_main vs main()
$ARGS += "-DSDL_MAIN_HANDLED"

# -------------------------
# 6. ВЫХОДНОЙ ФАЙЛ
# -------------------------
# куда сохранить exe
$ARGS += "-o"
$ARGS += $OUT

# =========================
# КОМПИЛЯЦИЯ
# =========================
# запускаем gcc со всеми аргументами
& $CC @ARGS

# если gcc упал — стопаемся
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "Build success" -ForegroundColor Green

# =========================
# DLL (динамические библиотеки)
# =========================
# SDL работает как DLL → без них exe не запустится

Copy-Item "$SDL/bin/SDL2.dll" $BUILD_DIR -Force
Copy-Item "$TTF/bin/SDL2_ttf.dll" $BUILD_DIR -Force

# =========================
# ЗАПУСК
# =========================
# Resolve-Path превращает путь в абсолютный
# потом запускаем exe

Write-Host "Running..." -ForegroundColor Green

& (Resolve-Path $OUT)