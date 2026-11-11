
#include "output.h"

void ledRedToggle(void) {
	HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
}

void ledYellowToggle(void) {
	HAL_GPIO_TogglePin(LED_YELLOW_GPIO_Port, LED_YELLOW_Pin);
}

void ledGreenToggle(void) {
	HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
}

void ledAquaToggle(void) {
	HAL_GPIO_TogglePin(LED_AQUA_GPIO_Port, LED_AQUA_Pin);
}

void ledBlueToggle(void) {
	HAL_GPIO_TogglePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
}

