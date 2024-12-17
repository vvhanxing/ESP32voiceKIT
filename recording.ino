#define TFT_BROWN 0x38E0
#include <TFT_eSPI.h>  // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include <driver/i2s.h>

// TFT 屏幕设置
TFT_eSPI tft = TFT_eSPI();

// I2S 设置
#define I2S_WS 9
#define I2S_SD 10
#define I2S_SCK 8
#define I2S_PORT I2S_NUM_0
#define bufferLen 240

int16_t sBuffer[bufferLen];  // 音频数据缓冲区

// 屏幕设置
#define TFT_WIDTH 240
#define TFT_HEIGHT 240

// 波形图设置
#define CENTER_Y (TFT_HEIGHT / 2)      // Y轴中心
#define SCALE_FACTOR 30                // 缩放因子，调整波形高度
#define BAR_WIDTH 4                    // 每个矩形条的宽度
#define BAR_SPACING 1                  // 矩形条之间的间隔
#define PEAK_DROP_SPEED 1              // 光点下落速度
#define BAR_DECAY_SPEED 2              // 矩形条降低速度

// 峰值下落效果
int peakPosition[TFT_WIDTH / (BAR_WIDTH + BAR_SPACING)];  // 峰值光点位置
int barHeights[TFT_WIDTH / (BAR_WIDTH + BAR_SPACING)];    // 每个矩形条的高度

void setup() {
  Serial.begin(115200);
  delay(1000);

  // I2S 初始化
  i2s_install();
  i2s_setpin();
  i2s_start(I2S_PORT);
  delay(500);

  // TFT 初始化
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // 初始化峰值和高度数组
  for (int i = 0; i < TFT_WIDTH / (BAR_WIDTH + BAR_SPACING); i++) {
    peakPosition[i] = CENTER_Y;
    barHeights[i] = 0;
  }

  Serial.println("Setup complete...");
}

void loop() {
  size_t bytesIn = 0;
  esp_err_t result = i2s_read(I2S_PORT, &sBuffer, bufferLen * sizeof(int16_t), &bytesIn, portMAX_DELAY);

  if (result == ESP_OK) {
    int samples_read = bytesIn / 2;
    if (samples_read > 0) {
      tft.fillScreen(TFT_BLACK);  // 清屏
      drawWaveform(samples_read);
    }
  }
}

void drawWaveform(int samples_read) {
  int numBars = TFT_WIDTH / (BAR_WIDTH + BAR_SPACING);  // 计算能绘制的矩形条数
  int x_pos = 0;

  for (int i = 0; i < numBars; i++) {
    // 获取数据并计算当前高度
    int sampleIndex = map(i, 0, numBars, 0, samples_read - 1);
    int sample = abs(sBuffer[sampleIndex]) / SCALE_FACTOR;
    if (sample > CENTER_Y) sample = CENTER_Y;

    // 矩形条衰减效果
    if (sample > barHeights[i]) {
      barHeights[i] = sample;  // 更新高度
    } else {
      barHeights[i] -= BAR_DECAY_SPEED;  // 缓慢衰减
      if (barHeights[i] < 0) barHeights[i] = 0;
    }

    // 绘制矩形条
    uint16_t barColor = colorGradient(i, numBars);
    tft.fillRect(x_pos, CENTER_Y - barHeights[i], BAR_WIDTH, barHeights[i], barColor);

    // 峰值光点效果
    if (CENTER_Y - barHeights[i] < peakPosition[i]) {
      peakPosition[i] = CENTER_Y - barHeights[i];  // 更新峰值位置
    } else {
      peakPosition[i] += PEAK_DROP_SPEED;  // 光点下落效果
      if (peakPosition[i] > CENTER_Y) peakPosition[i] = CENTER_Y;
    }

    // 绘制光点（4x4方块）
    tft.fillRect(x_pos, peakPosition[i] - 2, 4, 4, barColor);

    // 更新X位置
    x_pos += BAR_WIDTH + BAR_SPACING;
  }
}

// 生成颜色渐变函数（从红色色到紫色色）
uint16_t colorGradient(int position, int total) {
  float ratio = (float)position / total;

  // 红色到紫色：红色逐渐减少，蓝色逐渐增加
  int red = 255 - (127 * ratio);   // 红色从255减到128
  int green = 0;                  // 绿色分量固定为0
  int blue = 128 * ratio;         // 蓝色从0增加到128

  return tft.color565(red, green, blue);
}


// I2S 配置函数
void i2s_install() {
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,  // INMP441 推荐的采样率
    .bits_per_sample = i2s_bits_per_sample_t(16),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = bufferLen,
    .use_apll = false
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void i2s_setpin() {
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  i2s_set_pin(I2S_PORT, &pin_config);
}
