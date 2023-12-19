/*	Autor: Matheus Buratti
 * 	Build: west build -p auto -b nucleo_g431rb app
 * 	Flash: west flash
 */

// Zephyr imports
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/sys_clock.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/util.h>
#include <zephyr/zbus/zbus.h>

#include <stm32g431xx.h>

// Other libs
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include "arm_const_structs.h"

// =============================== LED ===============================

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)
#define SW0_NODE DT_ALIAS(sw0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

struct k_timer blinky_tm;

struct k_work blinky_wk;

// Liga e desliga o LED
void blinky_work_fn(struct k_work *blinky_wk)
{
	gpio_pin_toggle_dt(&led);
}

// Callback qnd timer expira
void blinky_expired_handler(struct k_timer *timer)
{
	k_work_submit(&blinky_wk);
}

K_TIMER_DEFINE(blinky_tm, blinky_expired_handler, NULL);

K_WORK_DEFINE(blinky_wk, blinky_work_fn);

// =============================== Button ===============================

struct k_thread keyboard_use_thread;
struct k_work_delayable keyboard_wk;
struct k_fifo button_queue;
int button_pressed;
static struct gpio_callback button_cb_data;
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});

// Callback botão
void button_pressed_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	// Guarda numa fila para ser executado mais tarde
	k_work_schedule(&keyboard_wk, K_MSEC(50));
}

// Work depois do delay para debounce
void keyboard_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	// Checa o botão após o debounce
	if (gpio_pin_get_dt(&button) == 1)
	{
		button_pressed = 0;
		k_fifo_put(&button_queue, &button_pressed);
	}
}

// Imprime informação da thread atual (é chamada para todas as threads)
void print_thread_info(const struct k_thread *thread, void *user_data)
{
	printk("\t%s\n", thread->name);
}

// Tarefa de teclado. Consome botões de uma fila para fazer ações
void keyboard_use(void)
{
	// Configuração botão
	gpio_pin_configure_dt(&button, GPIO_INPUT);
	gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&button_cb_data, button_pressed_cb, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);
	while (1)
	{
		int *key = (int *)k_fifo_get(&button_queue, K_NO_WAIT);
		if (key != NULL && *key == 0)
		{
			// Imprime as tarefas instaladas
			printk("Tarefas instaladas:\n");
			k_thread_foreach(print_thread_info, NULL);
		}

		k_msleep(100);
	}
}

K_FIFO_DEFINE(button_queue);

K_WORK_DELAYABLE_DEFINE(keyboard_wk, keyboard_work_fn);

K_THREAD_DEFINE(keyboard_use_th, 1024, keyboard_use, NULL, NULL, NULL, 7, 0, 0);

// ===============================  ZBUS ===============================

struct k_sem fft_sem, fft_print_sem;

K_SEM_DEFINE(fft_sem, 1, 1)
K_SEM_DEFINE(fft_print_sem, 1, 1)

struct adc_msg
{
	char ready;
};

ZBUS_CHAN_DEFINE(adc_ch,							  /* Name */
				 struct adc_msg,					  /* Message type */
				 NULL,								  /* Validator */
				 NULL,								  /* User data */
				 ZBUS_OBSERVERS(adc_handler_msg_sub), /* observers */
				 ZBUS_MSG_INIT(0)					  /* Initial value {0} */
);
ZBUS_SUBSCRIBER_DEFINE(adc_handler_msg_sub, 3);
// =============================== DAC/ADC ===============================

ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

DAC_HandleTypeDef hdac1;
DMA_HandleTypeDef hdma_dac1_ch1;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim8;

uint16_t sin_wave[256] = {2048, 2098, 2148, 2199, 2249, 2299, 2349, 2399, 2448, 2498, 2547, 2596, 2644, 2692, 2740, 2787, 2834, 2880, 2926, 2971, 3016, 3060, 3104, 3147, 3189, 3230, 3271, 3311, 3351, 3389, 3427, 3464, 3500, 3535, 3569, 3602, 3635, 3666, 3697, 3726, 3754, 3782, 3808, 3833, 3857, 3880, 3902, 3923, 3943, 3961, 3979, 3995, 4010, 4024, 4036, 4048, 4058, 4067, 4074, 4081, 4086, 4090, 4093, 4095, 4095, 4094, 4092, 4088, 4084, 4078, 4071, 4062, 4053, 4042, 4030, 4017, 4002, 3987, 3970, 3952, 3933, 3913, 3891, 3869, 3845, 3821, 3795, 3768, 3740, 3711, 3681, 3651, 3619, 3586, 3552, 3517, 3482, 3445, 3408, 3370, 3331, 3291, 3251, 3210, 3168, 3125, 3082, 3038, 2994, 2949, 2903, 2857, 2811, 2764, 2716, 2668, 2620, 2571, 2522, 2473, 2424, 2374, 2324, 2274, 2224, 2174, 2123, 2073, 2022, 1972, 1921, 1871, 1821, 1771, 1721, 1671, 1622, 1573, 1524, 1475, 1427, 1379, 1331, 1284, 1238, 1192, 1146, 1101, 1057, 1013, 970, 927, 885, 844, 804, 764, 725, 687, 650, 613, 578, 543, 509, 476, 444, 414, 384, 355, 327, 300, 274, 250, 226, 204, 182, 162, 143, 125, 108, 93, 78, 65, 53, 42, 33, 24, 17, 11, 7, 3, 1, 0, 0, 2, 5, 9, 14, 21, 28, 37, 47, 59, 71, 85, 100, 116, 134, 152, 172, 193, 215, 238, 262, 287, 313, 341, 369, 398, 429, 460, 493, 526, 560, 595, 631, 668, 706, 744, 784, 824, 865, 906, 948, 991, 1035, 1079, 1124, 1169, 1215, 1261, 1308, 1355, 1403, 1451, 1499, 1548, 1597, 1647, 1696, 1746, 1796, 1846, 1896, 1947, 1997, 2047};
uint16_t sin_wave_3rd_harmonic[256] = {2048, 2136, 2224, 2311, 2398, 2484, 2569, 2652, 2734, 2814, 2892, 2968, 3041, 3112, 3180, 3245, 3308, 3367, 3423, 3476, 3526, 3572, 3615, 3654, 3690, 3723, 3752, 3778, 3800, 3819, 3835, 3848, 3858, 3866, 3870, 3872, 3871, 3869, 3864, 3857, 3848, 3838, 3827, 3814, 3801, 3786, 3771, 3756, 3740, 3725, 3709, 3694, 3679, 3665, 3652, 3639, 3628, 3617, 3608, 3600, 3594, 3589, 3585, 3584, 3583, 3584, 3587, 3591, 3597, 3604, 3613, 3622, 3633, 3645, 3658, 3672, 3686, 3701, 3717, 3732, 3748, 3764, 3779, 3794, 3808, 3821, 3833, 3844, 3853, 3860, 3866, 3870, 3872, 3871, 3868, 3862, 3854, 3842, 3828, 3810, 3789, 3765, 3738, 3707, 3673, 3635, 3594, 3549, 3501, 3450, 3396, 3338, 3277, 3213, 3146, 3077, 3005, 2930, 2853, 2774, 2693, 2611, 2527, 2441, 2355, 2268, 2180, 2092, 2003, 1915, 1827, 1740, 1654, 1568, 1484, 1402, 1321, 1242, 1165, 1090, 1018, 949, 882, 818, 757, 699, 645, 594, 546, 501, 460, 422, 388, 357, 330, 306, 285, 267, 253, 241, 233, 227, 224, 223, 225, 229, 235, 242, 251, 262, 274, 287, 301, 316, 331, 347, 363, 378, 394, 409, 423, 437, 450, 462, 473, 482, 491, 498, 504, 508, 511, 512, 511, 510, 506, 501, 495, 487, 478, 467, 456, 443, 430, 416, 401, 386, 370, 355, 339, 324, 309, 294, 281, 268, 257, 247, 238, 231, 226, 224, 223, 225, 229, 237, 247, 260, 276, 295, 317, 343, 372, 405, 441, 480, 523, 569, 619, 672, 728, 787, 850, 915, 983, 1054, 1127, 1203, 1281, 1361, 1443, 1526, 1611, 1697, 1784, 1871, 1959, 2047};

uint16_t adcBuffer[256];
float mod[256];
float ReIm[256 * 2];

static void MX_ADC1_Init(void)
{

	/* USER CODE BEGIN ADC1_Init 0 */

	/* USER CODE END ADC1_Init 0 */

	ADC_MultiModeTypeDef multimode = {0};
	ADC_ChannelConfTypeDef sConfig = {0};

	/* USER CODE BEGIN ADC1_Init 1 */

	/* USER CODE END ADC1_Init 1 */

	/** Common config
	 */
	hadc1.Instance = ADC1;
	hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
	hadc1.Init.Resolution = ADC_RESOLUTION_12B;
	hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc1.Init.GainCompensation = 0;
	hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
	hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
	hadc1.Init.LowPowerAutoWait = DISABLE;
	hadc1.Init.ContinuousConvMode = DISABLE;
	hadc1.Init.NbrOfConversion = 1;
	hadc1.Init.DiscontinuousConvMode = DISABLE;
	hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T8_TRGO;
	hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
	hadc1.Init.DMAContinuousRequests = ENABLE;
	hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
	hadc1.Init.OversamplingMode = DISABLE;
	if (HAL_ADC_Init(&hadc1) != HAL_OK)
	{
		// Error_Handler();
	}

	/** Configure the ADC multi-mode
	 */
	multimode.Mode = ADC_MODE_INDEPENDENT;
	if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
	{
		// Error_Handler();
	}

	/** Configure Regular Channel
	 */
	sConfig.Channel = ADC_CHANNEL_1;
	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
	sConfig.SingleDiff = ADC_SINGLE_ENDED;
	sConfig.OffsetNumber = ADC_OFFSET_NONE;
	sConfig.Offset = 0;
	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
	{
		// Error_Handler();
	}
	/* USER CODE BEGIN ADC1_Init 2 */

	/* USER CODE END ADC1_Init 2 */
}

static void MX_DAC1_Init(void)
{

	/* USER CODE BEGIN DAC1_Init 0 */

	/* USER CODE END DAC1_Init 0 */

	DAC_ChannelConfTypeDef sConfig = {0};

	/* USER CODE BEGIN DAC1_Init 1 */

	/* USER CODE END DAC1_Init 1 */

	/** DAC Initialization
	 */
	hdac1.Instance = DAC1;
	if (HAL_DAC_Init(&hdac1) != HAL_OK)
	{
		// Error_Handler();
	}

	/** DAC channel OUT1 config
	 */
	sConfig.DAC_HighFrequency = DAC_HIGH_FREQUENCY_INTERFACE_MODE_AUTOMATIC;
	sConfig.DAC_DMADoubleDataMode = DISABLE;
	sConfig.DAC_SignedFormat = DISABLE;
	sConfig.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
	sConfig.DAC_Trigger = DAC_TRIGGER_T3_TRGO;
	sConfig.DAC_Trigger2 = DAC_TRIGGER_NONE;
	sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
	sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;
	sConfig.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
	if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
	{
		// Error_Handler();
	}
	/* USER CODE BEGIN DAC1_Init 2 */

	/* USER CODE END DAC1_Init 2 */
}

static void MX_TIM3_Init(void)
{

	/* USER CODE BEGIN TIM3_Init 0 */

	/* USER CODE END TIM3_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};

	/* USER CODE BEGIN TIM3_Init 1 */

	/* USER CODE END TIM3_Init 1 */
	htim3.Instance = TIM3;
	htim3.Init.Prescaler = 0;
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim3.Init.Period = 11067;
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
	{
		// Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
	{
		// Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
	{
		// Error_Handler();
	}
	/* USER CODE BEGIN TIM3_Init 2 */

	/* USER CODE END TIM3_Init 2 */
}

static void MX_TIM8_Init(void)
{

	/* USER CODE BEGIN TIM8_Init 0 */

	/* USER CODE END TIM8_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};

	/* USER CODE BEGIN TIM8_Init 1 */

	/* USER CODE END TIM8_Init 1 */
	htim8.Instance = TIM8;
	htim8.Init.Prescaler = 0;
	htim8.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim8.Init.Period = 11067;
	htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim8.Init.RepetitionCounter = 0;
	htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&htim8) != HAL_OK)
	{
		// Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim8, &sClockSourceConfig) != HAL_OK)
	{
		// Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
	sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim8, &sMasterConfig) != HAL_OK)
	{
		// Error_Handler();
	}
	/* USER CODE BEGIN TIM8_Init 2 */

	/* USER CODE END TIM8_Init 2 */
}

static void MX_DMA_Init(void)
{

	/* DMA controller clock enable */
	__HAL_RCC_DMAMUX1_CLK_ENABLE();
	__HAL_RCC_DMA1_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA1_Channel1_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
	/* DMA1_Channel2_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
}

DMA_HandleTypeDef hdma_adc1;

DMA_HandleTypeDef hdma_dac1_ch1;

DMA_HandleTypeDef hdma_adc1;
DMA_HandleTypeDef hdma_dac1_ch1;
UART_HandleTypeDef hlpuart1;
TIM_HandleTypeDef htim1;

void HAL_MspInit(void)
{
	/* USER CODE BEGIN MspInit 0 */

	/* USER CODE END MspInit 0 */

	__HAL_RCC_SYSCFG_CLK_ENABLE();
	__HAL_RCC_PWR_CLK_ENABLE();

	/* System interrupt init*/
	/* PendSV_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(PendSV_IRQn, 15, 0);

	/** Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral
	 */
	HAL_PWREx_DisableUCPDDeadBattery();

	/* USER CODE BEGIN MspInit 1 */

	/* USER CODE END MspInit 1 */
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
	if (hadc->Instance == ADC1)
	{
		/* USER CODE BEGIN ADC1_MspInit 0 */

		/* USER CODE END ADC1_MspInit 0 */

		/** Initializes the peripherals clocks
		 */
		PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC12;
		PeriphClkInit.Adc12ClockSelection = RCC_ADC12CLKSOURCE_SYSCLK;
		if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
		{
			//   Error_Handler();
		}

		/* Peripheral clock enable */
		__HAL_RCC_ADC12_CLK_ENABLE();

		__HAL_RCC_GPIOA_CLK_ENABLE();
		/**ADC1 GPIO Configuration
		PA0     ------> ADC1_IN1
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_0;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		/* ADC1 DMA Init */
		/* ADC1 Init */
		hdma_adc1.Instance = DMA1_Channel2;
		hdma_adc1.Init.Request = DMA_REQUEST_ADC1;
		hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
		hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
		hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
		hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
		hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
		hdma_adc1.Init.Mode = DMA_CIRCULAR;
		hdma_adc1.Init.Priority = DMA_PRIORITY_LOW;
		if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
		{
			//   Error_Handler();
		}

		__HAL_LINKDMA(hadc, DMA_Handle, hdma_adc1);

		/* USER CODE BEGIN ADC1_MspInit 1 */

		/* USER CODE END ADC1_MspInit 1 */
	}
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef *hadc)
{
	if (hadc->Instance == ADC1)
	{
		/* USER CODE BEGIN ADC1_MspDeInit 0 */

		/* USER CODE END ADC1_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_ADC12_CLK_DISABLE();

		/**ADC1 GPIO Configuration
		PA0     ------> ADC1_IN1
		*/
		HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0);

		/* ADC1 DMA DeInit */
		HAL_DMA_DeInit(hadc->DMA_Handle);
		/* USER CODE BEGIN ADC1_MspDeInit 1 */

		/* USER CODE END ADC1_MspDeInit 1 */
	}
}

void HAL_DAC_MspInit(DAC_HandleTypeDef *hdac)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	if (hdac->Instance == DAC1)
	{
		/* USER CODE BEGIN DAC1_MspInit 0 */

		/* USER CODE END DAC1_MspInit 0 */
		/* Peripheral clock enable */
		__HAL_RCC_DAC1_CLK_ENABLE();

		__HAL_RCC_GPIOA_CLK_ENABLE();
		/**DAC1 GPIO Configuration
		PA4     ------> DAC1_OUT1
		*/
		GPIO_InitStruct.Pin = GPIO_PIN_4;
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		/* DAC1 DMA Init */
		/* DAC1_CH1 Init */
		hdma_dac1_ch1.Instance = DMA1_Channel1;
		hdma_dac1_ch1.Init.Request = DMA_REQUEST_DAC1_CHANNEL1;
		hdma_dac1_ch1.Init.Direction = DMA_MEMORY_TO_PERIPH;
		hdma_dac1_ch1.Init.PeriphInc = DMA_PINC_DISABLE;
		hdma_dac1_ch1.Init.MemInc = DMA_MINC_ENABLE;
		hdma_dac1_ch1.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
		hdma_dac1_ch1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
		hdma_dac1_ch1.Init.Mode = DMA_CIRCULAR;
		hdma_dac1_ch1.Init.Priority = DMA_PRIORITY_LOW;
		if (HAL_DMA_Init(&hdma_dac1_ch1) != HAL_OK)
		{
			//   Error_Handler();
		}

		__HAL_LINKDMA(hdac, DMA_Handle1, hdma_dac1_ch1);

		/* USER CODE BEGIN DAC1_MspInit 1 */

		/* USER CODE END DAC1_MspInit 1 */
	}
}

void HAL_DAC_MspDeInit(DAC_HandleTypeDef *hdac)
{
	if (hdac->Instance == DAC1)
	{
		/* USER CODE BEGIN DAC1_MspDeInit 0 */

		/* USER CODE END DAC1_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_DAC1_CLK_DISABLE();

		/**DAC1 GPIO Configuration
		PA4     ------> DAC1_OUT1
		*/
		HAL_GPIO_DeInit(GPIOA, GPIO_PIN_4);

		/* DAC1 DMA DeInit */
		HAL_DMA_DeInit(hdac->DMA_Handle1);
		/* USER CODE BEGIN DAC1_MspDeInit 1 */

		/* USER CODE END DAC1_MspDeInit 1 */
	}
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim_base)
{
	if (htim_base->Instance == TIM3)
	{
		/* USER CODE BEGIN TIM3_MspInit 0 */

		/* USER CODE END TIM3_MspInit 0 */
		/* Peripheral clock enable */
		__HAL_RCC_TIM3_CLK_ENABLE();
		/* USER CODE BEGIN TIM3_MspInit 1 */

		/* USER CODE END TIM3_MspInit 1 */
	}
	else if (htim_base->Instance == TIM8)
	{
		/* USER CODE BEGIN TIM8_MspInit 0 */

		/* USER CODE END TIM8_MspInit 0 */
		/* Peripheral clock enable */
		__HAL_RCC_TIM8_CLK_ENABLE();
		/* USER CODE BEGIN TIM8_MspInit 1 */

		/* USER CODE END TIM8_MspInit 1 */
	}
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *htim_base)
{
	if (htim_base->Instance == TIM3)
	{
		/* USER CODE BEGIN TIM3_MspDeInit 0 */

		/* USER CODE END TIM3_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_TIM3_CLK_DISABLE();
		/* USER CODE BEGIN TIM3_MspDeInit 1 */

		/* USER CODE END TIM3_MspDeInit 1 */
	}
	else if (htim_base->Instance == TIM8)
	{
		/* USER CODE BEGIN TIM8_MspDeInit 0 */

		/* USER CODE END TIM8_MspDeInit 0 */
		/* Peripheral clock disable */
		__HAL_RCC_TIM8_CLK_DISABLE();
		/* USER CODE BEGIN TIM8_MspDeInit 1 */

		/* USER CODE END TIM8_MspDeInit 1 */
	}
}

void DMA1_Channel1_IRQHandler(void)
{
	/* USER CODE BEGIN DMA1_Channel1_IRQn 0 */

	/* USER CODE END DMA1_Channel1_IRQn 0 */
	HAL_DMA_IRQHandler(&hdma_dac1_ch1);
	/* USER CODE BEGIN DMA1_Channel1_IRQn 1 */

	/* USER CODE END DMA1_Channel1_IRQn 1 */
}

void DMA1_Channel2_IRQHandler(void)
{
	/* USER CODE BEGIN DMA1_Channel2_IRQn 0 */

	/* USER CODE END DMA1_Channel2_IRQn 0 */
	HAL_DMA_IRQHandler(&hdma_adc1);
	/* USER CODE BEGIN DMA1_Channel2_IRQn 1 */

	/* USER CODE END DMA1_Channel2_IRQn 1 */
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	k_sem_give(&fft_sem);
}

void fft_task(void)
{
	MX_DMA_Init();
	MX_ADC1_Init();
	MX_DAC1_Init();
	MX_TIM8_Init();
	MX_TIM3_Init();

	IRQ_CONNECT(DMA1_Channel1_IRQn, 5, DMA1_Channel1_IRQHandler, 0, 0);
	IRQ_CONNECT(DMA1_Channel2_IRQn, 5, DMA1_Channel2_IRQHandler, 0, 0);

	HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adcBuffer, 256);
	HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t *)sin_wave_3rd_harmonic, 256, DAC_ALIGN_12B_R);

	HAL_TIM_Base_Start(&htim8);
	HAL_TIM_Base_Start(&htim3);

	while (1)
	{
		k_sem_take(&fft_sem, K_FOREVER);
		int k = 0;
		for (int i = 0; i < 256; i++)
		{
			ReIm[k] = (float)adcBuffer[i] * 0.0008056640625;
			ReIm[k + 1] = 0.0;
			k += 2;
		}

		arm_cfft_f32(&arm_cfft_sR_f32_len256, ReIm, 0, 1);
		arm_cmplx_mag_f32(ReIm, mod, 256);
		arm_scale_f32(mod, 0.0078125, mod, 128);

		zbus_chan_pub(&adc_ch, &(struct adc_msg){.ready = 1}, K_FOREVER);

		// volatile float fund_phase = atan2f(ReIm[3], ReIm[2]) * 180 / M_PI;
		// (void)fund_phase;
	}
}

K_THREAD_DEFINE(fft_task_th, 1024, fft_task, NULL, NULL, NULL, 7, 0, 0);

struct fft_print_config
{
	int first_harm;
	int num_harm;
	char print;
};

struct fft_print_config fft_print_config = {.first_harm = 1, .num_harm = 3, .print = 0};

void fft_print_task(void)
{
	while (1)
	{
		const struct zbus_channel *chan;
		while (!zbus_sub_wait(&adc_handler_msg_sub, &chan, K_FOREVER))
		{
			struct adc_msg adc;
			zbus_chan_read(chan, &adc, K_MSEC(200));
			if (fft_print_config.print == 1)
			{
				fft_print_config.print = 0;
				printk("FFT result for the current DAC signal (%d, %d): ", fft_print_config.first_harm, fft_print_config.num_harm);
				for (int i = fft_print_config.first_harm; i < (fft_print_config.num_harm + fft_print_config.first_harm); i++)
				{
					if (i == 0)
					{
						printk("%f ", mod[i] / 2.0);
					}
					else
					{
						printk("%f ", mod[i]);
					}
				}
				printk("\n\r");
			}
		}
	}
}

K_THREAD_DEFINE(fft_print_task_th, 1024, fft_print_task, NULL, NULL, NULL, 7, 0, 0);
// =============================== Shell ===============================

static int cmd_ping(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "pong");

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(testes,
							   SHELL_CMD(ping, NULL, "Ping?", cmd_ping),
							   SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(teste, &testes, "Comandos de teste!", NULL);

static int cmd_tasks(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Tarefas instaladas:");
	k_thread_foreach(print_thread_info, NULL);
	return 0;
}

static int cmd_stack(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	const struct k_thread *current = k_current_get();

	size_t simple_usage, size = current->stack_info.size;

	k_thread_stack_space_get(current, &simple_usage);

	shell_print(sh, "Uso da pilha: %zu de %zu\n", simple_usage, size);

	return 0;
}

static int cmd_runtime(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(sh, "Estatísticas de runtime:");

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(my_log,
							   SHELL_CMD(tasks, NULL, "Mostra as tarefas instaladas", cmd_tasks),
							   SHELL_CMD(stack, NULL, "Mostra a pilha ocupada", cmd_stack),
							   SHELL_CMD(runtime, NULL, "Mostra estatísticas de runtime", cmd_runtime),
							   SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(log, &my_log, "Comandos de teste!", NULL);

static int cmd_sine(const struct shell *sh, size_t argc, char **argv)
{
	HAL_TIM_Base_Stop(&htim3);
	HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
	HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t *)sin_wave, 256, DAC_ALIGN_12B_R);
	HAL_TIM_Base_Start(&htim3);
	return 0;
}

static int cmd_sine3d(const struct shell *sh, size_t argc, char **argv)
{
	HAL_TIM_Base_Stop(&htim3);
	HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
	HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t *)sin_wave_3rd_harmonic, 256, DAC_ALIGN_12B_R);
	HAL_TIM_Base_Start(&htim3);
	return 0;
}

static int cmd_fft(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 3)
	{
		shell_print(sh, "Uso: fft first_harm num_harm");
		shell_print(sh, "Exemplo: fft %d %d", atoi(argv[1]), atoi(argv[2]));
		return 0;
	}
	int first_harm = atoi(argv[1]);
	int num_harm = atoi(argv[2]);
	if ((first_harm >= 0) && (num_harm > 0))
	{
		fft_print_config.first_harm = first_harm;
		fft_print_config.num_harm = num_harm;
		fft_print_config.print = 1;
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(dac,
							   SHELL_CMD(sine, NULL, "Sinal senoidal", cmd_sine),
							   SHELL_CMD(sine3d, NULL, "Sinal senoidal terceira harmonica", cmd_sine3d),
							   SHELL_CMD(fft, NULL, "FFT", cmd_fft),
							   SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(dac, &dac, "Comandos DAC", NULL);

int main(void)
{
	// Configuração led
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

	k_timer_start(&blinky_tm, K_MSEC(500), K_MSEC(500));
	return 0;
}