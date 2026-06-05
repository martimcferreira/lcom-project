/**
 * @file library.h
 * @brief Includes principais do projeto.
 *
 * Junta todos os drivers do MINIX num só ficheiro para ser mais fácil de dar include.
 * 
 * @defgroup Library Core Library Includes
 * @ingroup Devices
 * @brief Drivers do Minix.
 * @{
 */

#ifndef _PROJECT_LIBRARY_H_
#define _PROJECT_LIBRARY_H_

#include "drivers/bitwise.h"
#include "drivers/i8042.h"
#include "drivers/i8254.h"
#include "drivers/kbc.h"
#include "drivers/mouse.h"
#include "drivers/rtc.h"
#include "drivers/timer.h"
#include "drivers/utils.h"
#include "video.h"

#endif /* _PROJECT_LIBRARY_H_ */
/** @} */
