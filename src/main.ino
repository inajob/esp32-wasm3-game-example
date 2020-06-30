#include <FS.h>
#include <SPI.h>

#include <TFT_eSPI.h>
TFT_eSPI screen = TFT_eSPI();
TFT_eSprite tft = TFT_eSprite(&screen);
byte col[3] = {0,0,0};

#include <wasm3.h>

#include <m3_api_defs.h>
#include <m3_env.h>

#define WASM_STACK_SLOTS    2048
#define NATIVE_STACK_SIZE   (32*1024)

// For (most) devices that cannot allocate a 64KiB wasm page
//#define WASM_MEMORY_LIMIT   4096

#include "../wasm/app.wasm.h"

inline uint16_t rgb24to16(uint8_t r, uint8_t g, uint8_t b) {
  uint16_t tmp = ((r>>3) << 11) | ((g>>2) << 5) | (b>>3);
  return tmp; //(tmp >> 8) | (tmp << 8);
}

/*
 * API bindings
 *
 * Note: each RawFunction should complete with one of these calls:
 *   m3ApiReturn(val)   - Returns a value
 *   m3ApiSuccess()     - Returns void (and no traps)
 *   m3ApiTrap(trap)    - Returns a trap
 */

m3ApiRawFunction(m3_arduino_millis)
{
    m3ApiReturnType (uint32_t)

    m3ApiReturn(millis());
}

m3ApiRawFunction(m3_arduino_delay)
{
    m3ApiGetArg     (uint32_t, ms)

    // You can also trace API calls
    //Serial.print("api: delay "); Serial.println(ms);

    delay(ms);

    m3ApiSuccess();
}

m3ApiRawFunction(m3_arduino_color)
{
    m3ApiGetArg     (uint32_t, r)
    m3ApiGetArg     (uint32_t, g)
    m3ApiGetArg     (uint32_t, b)

    // You can also trace API calls
    //Serial.print("api: pset "); Serial.print(x); Serial.print(' '); Serial.println(y)

    col[0] = r;
    col[1] = g;
    col[2] = b;

    m3ApiSuccess();
}

m3ApiRawFunction(m3_arduino_pset)
{
    m3ApiGetArg     (uint32_t, x)
    m3ApiGetArg     (uint32_t, y)

    // You can also trace API calls
    //Serial.print("api: pset "); Serial.print(x); Serial.print(' '); Serial.println(y)

    tft.drawPixel(x, y, rgb24to16(col[0], col[1], col[2]));

    m3ApiSuccess();
}

m3ApiRawFunction(m3_arduino_print)
{
    m3ApiGetArgMem  (const uint8_t *, buf)
    m3ApiGetArg     (uint32_t,        len)

    //printf("api: print %p %d\n", buf, len);
    Serial.write(buf, len);

    m3ApiSuccess();
}

m3ApiRawFunction(m3_dummy)
{
    m3ApiSuccess();
}

M3Result  LinkArduino  (IM3Runtime runtime)
{
    IM3Module module = runtime->modules;
    const char* arduino = "arduino";

    m3_LinkRawFunction (module, arduino, "millis",           "i()",    &m3_arduino_millis);
    m3_LinkRawFunction (module, arduino, "delay",            "v(i)",   &m3_arduino_delay);
    m3_LinkRawFunction (module, arduino, "pset",             "v(ii)",  &m3_arduino_pset);
    m3_LinkRawFunction (module, arduino, "color",             "v(iii)",  &m3_arduino_color);

    // Test functions
    m3_LinkRawFunction (module, arduino, "print",            "v(*i)",  &m3_arduino_print);

    // Dummy (for TinyGo)
    m3_LinkRawFunction (module, "env",   "io_get_stdout",    "i()",    &m3_dummy);

    return m3Err_none;
}

#define FATAL(func, msg) { Serial.print("Fatal: " func " "); Serial.println(msg); return; }

IM3Environment env;
IM3Runtime runtime;
IM3Module module;
IM3Function func;

void wasm_task(void*)
{
    Serial.print("Start wasm_task");
    M3Result result = m3Err_none;

    env = m3_NewEnvironment ();
    if (!env) FATAL("NewEnvironment", "failed");

    runtime = m3_NewRuntime (env, WASM_STACK_SLOTS, NULL);
    if (!runtime) FATAL("NewRuntime", "failed");

#ifdef WASM_MEMORY_LIMIT
    runtime->memoryLimit = WASM_MEMORY_LIMIT;
#endif

    result = m3_ParseModule (env, &module, build_app_wasm, build_app_wasm_len-1);
    if (result) FATAL("ParseModule", result);

    result = m3_LoadModule (runtime, module);
    if (result) FATAL("LoadModule", result);

    result = LinkArduino (runtime);
    if (result) FATAL("LinkArduino", result);

    result = m3_FindFunction (&func, runtime, "_start");
    if (result) FATAL("FindFunction", result);

    Serial.println("Grab function...");

    while(true){
      // yet another loop function
      myLoop();
    }
}

const char* i_argv[1] = { NULL };
void myLoop(){
  M3Result result = m3Err_none;
  result = m3_CallWithArgs (func, 0, i_argv);
  if (result) FATAL("m3_CallWithArgs: %s", result);

  // == display update ==
  tft.setCursor(0,120);
  tft.setTextColor(0xffff);
  tft.print(ESP.getFreeHeap());
  tft.pushSprite(0, 0);
  delay(1);
}


void setup(){
  Serial.begin(115200);
  screen.init();
  screen.setRotation(0);
  tft.createSprite(128, 128);
  tft.setTextWrap(false);

  tft.fillScreen(TFT_BLACK);

  tft.setCursor(3, 6);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.pushSprite(0, 0);

  xTaskCreate(&wasm_task, "wasm3", NATIVE_STACK_SIZE, NULL, 5, NULL);
}

void loop(){
  delay(100);
}
