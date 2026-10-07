# Реализация работы с SPI FLASH и EEPROM для mik32
- Режим XIP, устанавливаемый после всех операций записи/стирания SPI флэша - на основе команды "Read Data Bytes (READ) (03H)", так как она обычно одинаково работает на флэшах разных производителей.
- Для всех "алгоритмов" в начале стоит команда call вызова функции main(), а следом команда ebreak. Так что с любым "алгоритмом" адрес точки выхода единый.

## В конфиге для openocd
```
# Work-area is a space in RAM used for flash programming
if { [info exists WORKAREASIZE] } {
   set _WORKAREASIZE $WORKAREASIZE
} else {
   set _WORKAREASIZE 0x4000
}

$_TARGETNAME configure -work-area-phys 0x02000000 -work-area-size $_WORKAREASIZE -work-area-backup 0

flash bank $_CHIPNAME.flash mik32_flash 0x80000000 0 0 16 $_TARGETNAME
flash bank $_CHIPNAME.eeprom mik32_eeprom 0x01000000 0 0 16 $_TARGETNAME
```
