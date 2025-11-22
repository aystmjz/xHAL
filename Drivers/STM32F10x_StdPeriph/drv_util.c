#include "drv_util.h"
#include <ctype.h>
#include <string.h>

void _gpio_clock_enable(const char *name)
{
    GPIO_TypeDef *port = _get_port_from_name(name);
    switch ((uintptr_t)port)
    {
    case (uintptr_t)GPIOA:
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
        break;
    case (uintptr_t)GPIOB:
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
        break;
    case (uintptr_t)GPIOC:
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
        break;
    case (uintptr_t)GPIOD:
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
        break;
    case (uintptr_t)GPIOE:
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
        break;
    case (uintptr_t)GPIOF:
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF, ENABLE);
        break;
    case (uintptr_t)GPIOG:
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOG, ENABLE);
        break;
    default:
        break;
    }
}

bool _check_pin_name_valid(const char *name)
{
    uint8_t len = strlen(name);
    if (len < 3 || len > 4)
    {
        return false;
    }

    if (toupper(name[0]) != 'P')
    {
        return false;
    }
    if (toupper(name[1]) < 'A' || toupper(name[1]) > 'G')
    {
        return false;
    }

    for (uint8_t i = 2; i < len; i++)
    {
        if (!(name[i] >= '0' && name[i] <= '9'))
        {
            return false;
        }
    }

    int8_t pin_num = atoi(&name[2]);
    if (pin_num < 0 || pin_num >= 16)
    {
        return false;
    }

    return true;
}

GPIO_TypeDef *_get_port_from_name(const char *name)
{
    static const GPIO_TypeDef *port_table[] = {
        GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG,
    };

    return (GPIO_TypeDef *)port_table[name[1] - 'A'];
}

uint16_t _get_pin_from_name(const char *name)
{
    char *str_num   = (char *)&name[2];
    uint8_t pin_num = atoi(str_num);

    return (uint16_t)(1 << pin_num);
}
