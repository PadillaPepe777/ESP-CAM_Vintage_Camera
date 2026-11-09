
#include <Arduino.h>

#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// MicroSD
#include "FS.h"
#include "SD_MMC.h"

// TFT
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <TJpg_Decoder.h>

//IMG_0000.JPG

// =====================================================
// CAMERA: AI THINKER ESP32-CAM
// =====================================================

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// =====================================================
// TFT: SOFTWARE SPI
// CS -> GND
// RST -> 3.3V
// Con esta configuracion se requiere cortar la energia al TFT para reiniciarlo
//Puede ser simplemente apagar y prender todo el circuito
// =====================================================

#define TFT_DC       12 //RS = DC
#define TFT_MOSI     13 // MOSI = SDA
#define TFT_SCLK      4 // SCLK = SCL = CLK

Adafruit_ST7735 tft(
    -1,         //Indica CS conectado a GND
    TFT_DC,
    TFT_MOSI,
    TFT_SCLK,
    -1          //Indica RST conectado a 3.3V
);

// =====================================================
// BUTTONS
// =====================================================

#define BUTTON_CAPTURE  1
#define BUTTON_GALLERY  3

// =====================================================
// CAMERA SETTINGS
// =====================================================

bool cameraReady = false;
bool sdReady = false;

// Resolución de la pantalla QQVGA: (160×120)
// Resolución de con esta configuracion cámara: VGA(640×480)
// Es mas lento en pantalla pero mejor resolucion al exportar la foto a la microSD
// se escala imagen en TFT con funcion TJpgDec.setJpgScale(4) por ser 4 veces mas grande que la pantalla

#define CAMERA_FRAME_SIZE FRAMESIZE_VGA

// =====================================================
// PHOTO MANAGEMENT
// =====================================================

#define MAX_PHOTOS 200

String photoFiles[MAX_PHOTOS];

int photoCount = 0;
int galleryIndex = 0;

uint32_t nextPhotoNumber = 1;

// =====================================================
// OPERATING MODES
// =====================================================

enum OperatingMode {
    MODE_LIVE,
    MODE_GALLERY
};

OperatingMode currentMode = MODE_LIVE;

// =====================================================
// TIMING
// =====================================================

unsigned long lastFrameTime = 0;
const unsigned long FRAME_INTERVAL = 120;

const unsigned long DEBOUNCE_TIME = 250;

unsigned long lastCaptureButtonTime = 0;
unsigned long lastGalleryButtonTime = 0;

bool lastCaptureState = HIGH;
bool lastGalleryState = HIGH;

// =====================================================
// FUNCTION PROTOTYPES
// =====================================================

bool initializeCamera();
bool initializeSD();

void refreshPhotoList();
void findNextPhotoNumber();

void capturePhoto();
void displayLiveFrame();
void displayStoredPhoto(int index);

void enterGallery();
void nextGalleryPhoto();

void handleButtons();
void showMessage(const char *line1, const char *line2);

// =====================================================
// JPEG CALLBACK
// =====================================================

bool tftOutput(
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t *bitmap
) {

    if (x >= tft.width() || y >= tft.height()) {
        return false;
    }

    tft.drawRGBBitmap(
        x,
        y,
        bitmap,
        w,
        h
    );

    return true;
}

// =====================================================
// DISPLAY MESSAGE
// =====================================================

void showMessage(
    const char *line1,
    const char *line2
) {

    tft.fillScreen(ST77XX_BLACK);

    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(1);

    tft.setCursor(5, 45);
    tft.println(line1);

    tft.setCursor(5, 65);
    tft.println(line2);
}

// =====================================================
// CAMERA INITIALIZATION
// =====================================================

bool initializeCamera() {

    camera_config_t config = {};

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;

    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;

    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;

    // Conservamos los nombres utilizados
    // por el código original funcional.
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;

    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = 16000000;

    config.pixel_format = PIXFORMAT_JPEG;

    // Resolución reducida para la TFT.
    config.frame_size = CAMERA_FRAME_SIZE;
    //Se puede variar para mejorar calidad o velocidad de captura
    config.jpeg_quality = 20; // Calidad JPEG (0-63, menor es más calidad)

    if (psramFound()) {
        config.fb_count = 2;
    } else {
        config.fb_count = 1;
    }

    Serial.println("Initializing camera...");

    esp_err_t err = esp_camera_init(&config);

    if (err != ESP_OK) {

        Serial.printf(
            "Camera init failed: 0x%x\n",
            err
        );

        return false;
    }

    sensor_t *sensor = esp_camera_sensor_get();

    if (sensor != nullptr) {

        sensor->set_framesize(
            sensor,
            CAMERA_FRAME_SIZE
        );

        sensor->set_brightness(sensor, 0);
        sensor->set_contrast(sensor, 0);
        sensor->set_saturation(sensor, 0);

        sensor->set_whitebal(sensor, 1);
        sensor->set_awb_gain(sensor, 1);
        sensor->set_exposure_ctrl(sensor, 1);
        sensor->set_gain_ctrl(sensor, 1);

        sensor->set_hmirror(sensor, 0);
        sensor->set_vflip(sensor, 0);
    }

    Serial.println("CAMERA OK");

    return true;
}

// =====================================================
// SD INITIALIZATION - 1 BIT MODE
// =====================================================

bool initializeSD() {

    Serial.println("Initializing SD card (1-bit)...");

    // true = modo 1-bit
    // false = no formatear automáticamente
    if (!SD_MMC.begin("/sdcard", true, false)) {

        Serial.println("SD MOUNT FAILED");

        return false;
    }

    if (SD_MMC.cardType() == CARD_NONE) {

        Serial.println("No SD card detected");

        return false;
    }

    Serial.printf(
        "SD size: %llu MB\n",
        SD_MMC.cardSize() / (1024 * 1024)
    );

    Serial.println("SD CARD OK");

    return true;
}

// =====================================================
// FIND NEXT AVAILABLE PHOTO NUMBER
// =====================================================

void findNextPhotoNumber() {

    uint32_t highestNumber = 0;

    File root = SD_MMC.open("/");

    if (!root) {
        nextPhotoNumber = 1;
        return;
    }

    File file = root.openNextFile();

    while (file) {

        String name = String(file.name());

        int start = name.lastIndexOf("IMG_");
        int end = name.lastIndexOf(".JPG");

        if (start >= 0 && end > start) {

            String numberText = name.substring(
                start + 4,
                end
            );

            uint32_t number = numberText.toInt();

            if (number > highestNumber) {
                highestNumber = number;
            }
        }

        file.close();

        file = root.openNextFile();
    }

    root.close();

    nextPhotoNumber = highestNumber + 1;

    Serial.printf(
        "Next photo number: %lu\n",
        (unsigned long)nextPhotoNumber
    );
}

// =====================================================
// GENERATE PHOTO NAME
// =====================================================

String createPhotoFilename(uint32_t number) {

    char filename[32];

    snprintf(
        filename,
        sizeof(filename),
        "/IMG_%04lu.JPG",
        (unsigned long)number
    );

    return String(filename);
}

// =====================================================
// CAPTURE AND SAVE PHOTO
// =====================================================

void capturePhoto() {

    if (!cameraReady || !sdReady) {

        showMessage(
            "Capture unavailable",
            "Check camera / SD"
        );

        delay(1000);

        return;
    }

    showMessage(
        "Capturing...",
        "Please wait"
    );

    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb) {

        showMessage(
            "Camera error",
            "Capture failed"
        );

        delay(1000);

        return;
    }

    if (fb->format != PIXFORMAT_JPEG) {

        esp_camera_fb_return(fb);

        showMessage(
            "Format error",
            "JPEG required"
        );

        delay(1000);

        return;
    }

    String filename = createPhotoFilename(
        nextPhotoNumber
    );

    File photo = SD_MMC.open(
        filename.c_str(),
        FILE_WRITE
    );

    if (!photo) {

        esp_camera_fb_return(fb);

        showMessage(
            "SD error",
            "Cannot create file"
        );

        delay(1000);

        return;
    }

    size_t expectedBytes = fb->len;

    size_t bytesWritten = photo.write(
        fb->buf,
        expectedBytes
    );

    photo.close();

    esp_camera_fb_return(fb);

    if (bytesWritten == expectedBytes) {

        nextPhotoNumber++;

        Serial.printf(
            "Photo saved: %s (%u bytes)\n",
            filename.c_str(),
            (unsigned int)bytesWritten
        );

        showMessage(
            "PHOTO SAVED!",
            filename.c_str()
        );

    } else {

        Serial.printf(
            "Write error: %u / %u bytes\n",
            (unsigned int)bytesWritten,
            (unsigned int)expectedBytes
        );

        showMessage(
            "Write error",
            "Incomplete photo"
        );
    }

    delay(1000);

    refreshPhotoList();
}

// =====================================================
// REFRESH PHOTO LIST
// NEWEST PHOTO FIRST
// =====================================================
void refreshPhotoList() {

    photoCount = 0;

    if (!sdReady) {
        return;
    }

    File root = SD_MMC.open("/");

    if (!root) {
        return;
    }

    File file = root.openNextFile();

    while (file && photoCount < MAX_PHOTOS) {

        if (!file.isDirectory()) {

            String name = String(file.name());

            // Obtener únicamente el nombre del archivo.
            int lastSlash = name.lastIndexOf('/');

            if (lastSlash >= 0) {
                name = name.substring(lastSlash + 1);
            }

            // Filtrar fotografías.
            if (
                name.startsWith("IMG_") &&
                name.endsWith(".JPG")
            ) {

                // Reconstruir la ruta desde la raíz.
                photoFiles[photoCount] = "/" + name;

                Serial.printf(
                    "Photo found: %s\n",
                    photoFiles[photoCount].c_str()
                );

                photoCount++;
            }
        }

        file.close();

        file = root.openNextFile();
    }

    root.close();

    // Ordenar de más reciente a más antigua.
    for (int i = 1; i < photoCount; i++) {

        String current = photoFiles[i];

        int j = i - 1;

        while (
            j >= 0 &&
            photoFiles[j] < current
        ) {

            photoFiles[j + 1] = photoFiles[j];

            j--;
        }

        photoFiles[j + 1] = current;
    }

    Serial.printf(
        "Photos found: %d\n",
        photoCount
    );
}

// =====================================================
// DISPLAY PHOTO FROM SD
// =====================================================

void displayStoredPhoto(int index) {

    if (index < 0 || index >= photoCount) {
        return;
    }

    String filename = photoFiles[index];

    Serial.printf(
        "Opening photo: %s\n",
        filename.c_str()
    );

    File photo = SD_MMC.open(
        filename.c_str(),
        FILE_READ
    );

    if (!photo) {

        Serial.println("ERROR: Cannot open file");

        showMessage(
            "Read error",
            "Cannot open photo"
        );

        return;
    }

    size_t fileSize = photo.size();

    Serial.printf(
        "File size: %u bytes\n",
        (unsigned int)fileSize
    );

    if (fileSize == 0) {

        photo.close();

        showMessage(
            "Invalid image",
            "Empty file"
        );

        return;
    }

    uint8_t *buffer = nullptr;

    if (psramFound()) {
        buffer = (uint8_t *)ps_malloc(fileSize);
    }

    if (buffer == nullptr) {
        buffer = (uint8_t *)malloc(fileSize);
    }

    if (buffer == nullptr) {

        photo.close();

        showMessage(
            "Memory error",
            "Cannot load photo"
        );

        return;
    }

    size_t bytesRead = photo.read(
        buffer,
        fileSize
    );

    photo.close();

    Serial.printf(
        "Bytes read: %u / %u\n",
        (unsigned int)bytesRead,
        (unsigned int)fileSize
    );

    //Aqui empieza a desplegar la imagen en la pantalla TFT, se limpia la pantalla y se dibuja la imagen

    tft.fillScreen(ST77XX_BLACK);

    if (bytesRead == fileSize) {

        TJpgDec.drawJpg(
            0,
            4,
            buffer,
            fileSize
        );
//Dibujamos un rectangulo azul como indicador de que es Modo Galeria y no Captura de Foto
        tft.fillRect(
            0,
            0,
            160,
            4,
            ST77XX_BLUE
        );

    } else {

        showMessage(
            "Read error",
            "Incomplete photo"
        );
    }

    free(buffer);
}
// =====================================================
// ENTER GALLERY
// =====================================================

void enterGallery() {

    refreshPhotoList();

    if (photoCount == 0) {

        showMessage(
            "Gallery empty",
            "No photos found"
        );

        delay(1000);

        currentMode = MODE_LIVE;

        return;
    }

    currentMode = MODE_GALLERY;

    // Índice cero = foto más reciente.
    galleryIndex = 0;

    displayStoredPhoto(galleryIndex);
}

// =====================================================
// NEXT PHOTO
// =====================================================

void nextGalleryPhoto() {

    if (photoCount == 0) {
        return;
    }

    galleryIndex++;

    if (galleryIndex >= photoCount) {
        galleryIndex = 0;
    }

    displayStoredPhoto(galleryIndex);
}

// =====================================================
// BUTTON MANAGEMENT
// =====================================================

void handleButtons() {

    bool captureState = digitalRead(BUTTON_CAPTURE);
    bool galleryState = digitalRead(BUTTON_GALLERY);

    unsigned long now = millis();

    // -----------------------------------------------
    // CAPTURE BUTTON
    // -----------------------------------------------

    if (
        captureState == LOW &&
        lastCaptureState == HIGH &&
        now - lastCaptureButtonTime >= DEBOUNCE_TIME
    ) {

        lastCaptureButtonTime = now;

        if (currentMode == MODE_LIVE) {

            capturePhoto();

        } else {

            // Regresar a la vista en vivo.
            currentMode = MODE_LIVE;

            tft.fillScreen(ST77XX_BLACK);
        }
    }

    // -----------------------------------------------
    // GALLERY BUTTON
    // -----------------------------------------------

    if (
        galleryState == LOW &&
        lastGalleryState == HIGH &&
        now - lastGalleryButtonTime >= DEBOUNCE_TIME
    ) {

        lastGalleryButtonTime = now;

        if (currentMode == MODE_LIVE) {

            enterGallery();

        } else {

            nextGalleryPhoto();
        }
    }

    lastCaptureState = captureState;
    lastGalleryState = galleryState;
}

// =====================================================
// DISPLAY LIVE CAMERA FRAME
// =====================================================

void displayLiveFrame() {

    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb) {
        return;
    }

    if (fb->format == PIXFORMAT_JPEG) {

        TJpgDec.drawJpg(
            0,
            4,
            fb->buf,
            fb->len
        );
    }

    esp_camera_fb_return(fb);
}

// =====================================================
// SETUP
// =====================================================

void setup() {

    // Desactivar brownout detector,
    // igual que en el código original.
    WRITE_PERI_REG(
        RTC_CNTL_BROWN_OUT_REG,
        0
    );

    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("==========================");
    Serial.println("ESP32-CAM LOCAL CAMERA");
    Serial.println("==========================");

    // Inicializar botones.
    pinMode(BUTTON_CAPTURE, INPUT_PULLUP);
    pinMode(BUTTON_GALLERY, INPUT_PULLUP);

    // Inicializar TFT.
    tft.initR(INITR_BLACKTAB);

    tft.setRotation(1);

    tft.fillScreen(ST77XX_BLACK);

    TJpgDec.setCallback(tftOutput);
    TJpgDec.setJpgScale(4);
    TJpgDec.setSwapBytes(false);

    showMessage(
        "LOCAL CAMERA",
        "Starting..."
    );

    // Inicializar cámara.
    cameraReady = initializeCamera();

    // Inicializar microSD en 1-bit.
    sdReady = initializeSD();

    if (sdReady) {

        findNextPhotoNumber();

        refreshPhotoList();
    }

    // Mostrar estado de periféricos.
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(1);

    tft.setCursor(5, 35);

    tft.setTextColor(
        cameraReady ? ST77XX_GREEN : ST77XX_RED
    );

    tft.println(
        cameraReady ? "CAMERA OK" : "CAMERA ERROR"
    );

    tft.setCursor(5, 55);

    tft.setTextColor(
        sdReady ? ST77XX_GREEN : ST77XX_RED
    );

    tft.println(
        sdReady ? "SD CARD OK" : "SD CARD ERROR"
    );

    delay(1500);

    if (!cameraReady) {

        showMessage(
            "Camera error",
            "Check OV2640"
        );

    } else if (!sdReady) {

        showMessage(
            "SD error",
            "Check microSD"
        );

    } else {

        tft.fillScreen(ST77XX_BLACK);

        Serial.println("System ready");
    }
}

// =====================================================
// LOOP
// =====================================================

void loop() {

    handleButtons();

    if (
        currentMode == MODE_LIVE &&
        cameraReady
    ) {

        unsigned long now = millis();

        if (
            now - lastFrameTime >= FRAME_INTERVAL
        ) {

            lastFrameTime = now;

            displayLiveFrame();
        }
    }
}
