#include <SD.h>

#ifndef EEPROMDEKL
#define EEPROMDEKL

#include <EEPROM.h>


uint16_t ee_ptr_adress;
uint16_t ee_sekce_base[11];

typedef enum
{
	cas1_en = 0,
	tep1_en = 2,
	cas2_en = 4,
	tep2_en = 6,
	cas3_en = 8,
	tep3_en = 10,
	cas4_en = 12,
	tep4_en = 14,
	cas5_en = 16,
	tep5_en = 18,
}Ee_tab_offset_enum;

typedef enum
{
	eeAdr_en = 0,
	blokovani_en = 2,
	currTemp_en = 3,
	teplAkt_en = 5,
	vlhkAkt_en = 7,
	teplRuc_en = 9,
	offset_en = 11,
	nazev_en = 12,
	vystup_prew_en = 22,
	vystup_new_en = 24,
	outPin_en = 26,
	inPin_en = 27,
	tables_en = 28

}ee_offset_enum;

/*typedef enum 
{
	eeAdr_en=0,
	t1zap=2,
	t1vyp=4,
	t2zap = 6,
	t3vyp = 8,
}ee_spin_enum;

typedef enum
{
	eeAdr_en = 0,
	tFrom=2,
	tTo=4,
	enable=6

} fve_en_enum;*/
#endif
