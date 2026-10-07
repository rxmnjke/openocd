#ifndef PAD_CONFIG_H_INCLUDED
#define PAD_CONFIG_H_INCLUDED

#define PAD_CONFIG_PIN_M(pin) (0b11 << ((pin) << 1))
#define PAD_CONFIG_PIN(pin, value) ((value) << ((pin) << 1))

#define PAD_CONFIG_PUPD_NONE      0
#define PAD_CONFIG_PUPD_UP        1
#define PAD_CONFIG_PUPD_DOWN      2

#ifndef __ASSEMBLER__
    #include <stdint.h>
    
    typedef struct {
      uint32_t CFG; 
      uint32_t DS; 
      uint32_t PUPD;
    } PAD_CONFIG_PORT_TypeDef;

    typedef struct
    {
        volatile uint32_t PORT_0_CFG; 
        volatile uint32_t PORT_0_DS; 
        volatile uint32_t PORT_0_PUPD;
        
        volatile uint32_t PORT_1_CFG; 
        volatile uint32_t PORT_1_DS; 
        volatile uint32_t PORT_1_PUPD; 
        
        volatile uint32_t PORT_2_CFG; 
        volatile uint32_t PORT_2_DS; 
        volatile uint32_t PORT_2_PUPD;         
    } PAD_CONFIG_TypeDef;
    
    typedef struct {
      volatile PAD_CONFIG_PORT_TypeDef PORT[3];
    } PAD_CONFIG_PORTS_TypeDef;

#endif
    
    
    
#endif // PAD_CONFIG_H_INCLUDED
