/**
 * @brief Transparency color macros
 * 
 * Define the color that is to be considered as transparency for a given output format
 */
#define TRANSPARENCY_COLOR_1_5_5_5 0x8000
#define TRANSPARENCY_COLOR_8_8_8_8 0xFF000000
#define CHROMA_KEY_GREEN_888 0x00b140
#define CHROMA_KEY_GREEN_565 0x0588

/**
 * @brief Output formats supported by xpm_load() function
 * 
 * XPM_INDEXED can only be used for XPM pixmaps encoded using indexed color mode.
 * INVALID_XPM is set when the XPM pixmap is not valid.
 */
enum xpm_image_type {
  XPM_INDEXED,
  XPM_1_5_5_5,
  XPM_5_6_5,
  XPM_8_8_8,
  XPM_8_8_8_8,
  XPM_GRAY_1_5_5_5,
  XPM_GRAY_5_6_5,
  XPM_GRAY_8_8_8,
  XPM_GRAY_8_8_8_8,
  INVALID_XPM
};

/**
 * @brief Struct that stores the information about an image.
 * 
 * type is the output format
 * width is the number of horizontal pixels
 * height is the number of vertical pixels
 * size is the number of bytes [width * height * bytes_per_pixel]
 * data is the pointer to the start of the image data
 */
typedef struct {
  enum xpm_image_type type;
  uint16_t width;
  uint16_t height;
  size_t size;
  uint8_t *bytes;
} xpm_image_t;