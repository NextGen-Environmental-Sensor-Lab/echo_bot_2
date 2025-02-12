/*
 * Copyright (c) 2022 Libre Solar Technologies GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include <string.h>

/* change this to any other UART peripheral if desired */
/*#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart) */
#define DEV_CONSOLE DT_NODELABEL(uart0)
#define DEV_OTHER DT_NODELABEL(uart1)

#define MSG_SIZE 32

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart0_msgq, MSG_SIZE, 10, 4);
K_MSGQ_DEFINE(uart1_msgq, MSG_SIZE, 10, 4);

static const struct device *const my_uart0 = DEVICE_DT_GET(DEV_CONSOLE);
static const struct device *const my_uart1 = DEVICE_DT_GET(DEV_OTHER);

/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;

/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void uart0_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(my_uart0))
	{
		return;
	}

	if (!uart_irq_rx_ready(my_uart0))
	{
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(my_uart0, &c, 1) == 1)
	{
		if ((c == '\n' || c == '\r') && rx_buf_pos > 0)
		{
			/* terminate string */
			rx_buf[rx_buf_pos] = '\0';

			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart0_msgq, &rx_buf, K_NO_WAIT);

			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
		}
		else if (rx_buf_pos < (sizeof(rx_buf) - 1))
		{
			rx_buf[rx_buf_pos++] = c;
		}
	}
}

void uart1_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(my_uart1))
	{
		return;
	}

	if (!uart_irq_rx_ready(my_uart1))
	{
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(my_uart1, &c, 1) == 1)
	{
		if ((c == '\n' || c == '\r') && rx_buf_pos > 0)
		{
			/* terminate string */
			rx_buf[rx_buf_pos] = '\0';

			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart1_msgq, &rx_buf, K_NO_WAIT);

			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
		}
		else if (rx_buf_pos < (sizeof(rx_buf) - 1))
		{
			rx_buf[rx_buf_pos++] = c;
		}
		/* else: characters beyond buffer size are dropped */
	}
}
/*
 * Print a null-terminated string character by character to the UART interface
 */

void print_uart0(char *buf)
{
	int msg_len = strlen(buf);

	for (int i = 0; i < msg_len; i++)
	{
		uart_poll_out(my_uart0, buf[i]);
	}
}

void print_uart1(char *buf)
{
	int msg_len = strlen(buf);

	for (int i = 0; i < msg_len; i++)
	{
		uart_poll_out(my_uart1, buf[i]);
	}
}

int main(void)
{
	char tx_buf[MSG_SIZE];

	if (!device_is_ready(my_uart0))
	{
		printk("UART0 device not found!");
		return 0;
	}

	// configure interrupt and callback to receive data
	int ret = uart_irq_callback_user_data_set(my_uart0, uart0_cb, NULL);

	if (ret < 0)
	{
		if (ret == -ENOTSUP)
		{
			printk("Interrupt-driven UART0 API support not enabled\n");
		}
		else if (ret == -ENOSYS)
		{
			printk("UART0 device does not support interrupt-driven API\n");
		}
		else
		{
			printk("Error setting UART0 callback: %d\n", ret);
		}
		return 0;
	}
	if (!device_is_ready(my_uart0))
	{
		printk("UART0 device not found!");
		return 0;
	}

	if (!device_is_ready(my_uart1))
	{
		printk("UART1 device not found!");
		return 0;
	}

	// configure interrupt and callback to receive data
	int ret1 = uart_irq_callback_user_data_set(my_uart1, uart1_cb, NULL);

	if (ret1 < 0)
	{
		if (ret1 == -ENOTSUP)
		{
			printk("Interrupt-driven UART1 API support not enabled\n");
		}
		else if (ret1 == -ENOSYS)
		{
			printk("UART1 device does not support interrupt-driven API\n");
		}
		else
		{
			printk("Error setting UART1 callback: %d\n", ret1);
		}
		return 0;
	}
	if (!device_is_ready(my_uart0))
	{
		printk("UART0 device not found!");
		return 0;
	}

	uart_irq_rx_enable(my_uart0);
	uart_irq_rx_enable(my_uart1);

	print_uart0("UART0: Write something and and press enter to send to uart1:\r\n");
	print_uart1("UART1: Write something and and press enter to send to uart0:\r\n");

	while (1)
	{

		if (k_msgq_get(&uart1_msgq, &tx_buf, K_NO_WAIT) == 0)
		{
			print_uart0("mssg from uart1: ");
			print_uart0(tx_buf);
			print_uart0("\r\n");
		}

		if (k_msgq_get(&uart0_msgq, &tx_buf, K_NO_WAIT) == 0)
		{
			print_uart1("mssg from uart0: ");
			print_uart1(tx_buf);
			print_uart1("\r\n");
		}
	}
	return 0;
}
