#pragma once
#include "ztl/zalloc.h"
#include "common/dbbasic.h"
#include "pch.h"




#pragma pack(push, 1)
struct EquipSkill {

    // EquipSkill stands for additional skill attached on equipment

    EquipSkill::EquipSkill();
    EquipSkill::EquipSkill(int, int, int, _FILETIME);

    int nSkillID;               // Skill id
    int nSkillLevel;            // Current skill level
    FILETIME tDateExpire;      // Show expire date if set, like cash item. Set to 0 (64bits) for infinite equip skill
};

struct GW_ItemSlotEquip : GW_ItemSlotBase {

	uint8_t padding[0x139 - sizeof(GW_ItemSlotBase)];

	EquipSkill equipskill;

};
static_assert(sizeof(GW_ItemSlotEquip) == 0x139 + sizeof(EquipSkill));
#pragma pack(pop)

EquipSkill::EquipSkill() {
    this->nSkillID = 0;
    this->nSkillLevel = 0;
    this->tDateExpire.dwLowDateTime = 0;
    this->tDateExpire.dwHighDateTime = 0;
}

EquipSkill::EquipSkill(int skill_id, int skill_level, int skill_display_type, _FILETIME dateExpire) {
    this->nSkillID = skill_id;
    this->nSkillLevel = skill_level;
    this->tDateExpire = dateExpire;
}