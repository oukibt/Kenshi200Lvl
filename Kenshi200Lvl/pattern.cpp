#include <pattern.h>
#include <utils.h>
#include <hook.h>

bool LooksLikeHeapPtr(uintptr_t p)
{
    if (p < 0x10000 || p > 0x00007FFFFFFFFFFFULL) return false;
    if (p >= 0x7FF000000000ULL && p < 0x800000000000ULL) return false;
    if ((p & 0x7) != 0) return false;
    return true;
}

const char* SkillName(Skill s)
{
    switch (s) {
        case Skill::Strength: return "Strength";
        case Skill::Fitness: return "Fitness";
        case Skill::Dexterity: return "Dexterity";
        case Skill::Perception: return "Perception";
        case Skill::Toughness: return "Toughness";
        case Skill::Athletics: return "Athletics";
        case Skill::Medic: return "Medic";
        case Skill::MassCombat: return "MassCombat";
        case Skill::ArrowDefence: return "ArrowDefence";
        case Skill::Stealth: return "Stealth";
        case Skill::Swimming: return "Swimming";
        case Skill::Thieving: return "Thieving";
        case Skill::Lockpicking: return "Lockpicking";
        case Skill::Bluff: return "Bluff";
        case Skill::Assassin: return "Assassin";
        case Skill::Survival: return "Survival";
        case Skill::Tracking: return "Tracking";
        case Skill::Climbing: return "Climbing";
        case Skill::Doctor: return "Doctor";
        case Skill::Engineer: return "Engineer";
        case Skill::WeaponSmith: return "WeaponSmith";
        case Skill::ArmourSmith: return "ArmourSmith";
        case Skill::BowSmith: return "BowSmith";
        case Skill::Robotics: return "Robotics";
        case Skill::Science: return "Science";
        case Skill::Labouring: return "Labouring";
        case Skill::Farming: return "Farming";
        case Skill::Cooking: return "Cooking";
        case Skill::Dodging: return "Dodging";
        case Skill::FriendlyFire: return "FriendlyFire";
        case Skill::Katanas: return "Katanas";
        case Skill::Sabres: return "Sabres";
        case Skill::Hackers: return "Hackers";
        case Skill::Blunt: return "Blunt";
        case Skill::HeavyWeapons: return "HeavyWeapons";
        case Skill::Unarmed: return "Unarmed";
        case Skill::Bows: return "Bows";
        case Skill::Turrets: return "Turrets";
        case Skill::Polearms: return "Polearms";
        case Skill::CurrentItemMaximumJuryRig: return "JuryRig";
        case Skill::MeleeAttack: return "MeleeAttack";
        case Skill::MeleeDefence: return "MeleeDefence";
        default: return "Unknown";
    }
}

bool ResolveFromStatPtr(float* valuePointer, uintptr_t& stats, uintptr_t& character, Skill& skill)
{
    stats = 0;
    character = 0;
    skill = Skill::Unknown;

    if (!valuePointer) return false;
    const uintptr_t addr = reinterpret_cast<uintptr_t>(valuePointer);

    for (int off : kSkillOffs)
    {
        const uintptr_t cand = addr - static_cast<uintptr_t>(off);
        if (cand < 0x10000 || (cand & 0x7) != 0) continue;

        uintptr_t me = 0;
        if (!SafeRead(cand + 0x10, me) || !LooksLikeHeapPtr(me)) continue;

        stats = cand;
        character = me;
        skill = static_cast<Skill>(off);

        return true;
    }

    return false;
}

bool ReadSkill(uintptr_t stats, Skill skill, float& out)
{
    out = 0.f;
    if (!stats || skill == Skill::Unknown)
        return false;
    return SafeRead(stats + static_cast<int>(skill), out);
}

bool WriteSkill(uintptr_t stats, Skill skill, float value)
{
    if (!stats || skill == Skill::Unknown)
        return false;
    __try {
        *reinterpret_cast<float*>(stats + static_cast<int>(skill)) = value;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool ReadAllSkills(uintptr_t stats, AllSkills& s)
{
    s = {};
    if (!stats)
        return false;

    bool ok = true;
    ok &= SafeRead(stats + 0x080, s.strength);
    ok &= SafeRead(stats + 0x084, s.fitness);
    ok &= SafeRead(stats + 0x088, s.dexterity);
    ok &= SafeRead(stats + 0x08C, s.perception);
    ok &= SafeRead(stats + 0x090, s.toughness);
    ok &= SafeRead(stats + 0x094, s.athletics);
    ok &= SafeRead(stats + 0x098, s.medic);
    ok &= SafeRead(stats + 0x09C, s.massCombat);
    ok &= SafeRead(stats + 0x0A0, s.arrowDefence);
    ok &= SafeRead(stats + 0x0A4, s.stealth);
    ok &= SafeRead(stats + 0x0A8, s.swimming);
    ok &= SafeRead(stats + 0x0AC, s.thieving);
    ok &= SafeRead(stats + 0x0B0, s.lockpicking);
    ok &= SafeRead(stats + 0x0B4, s.bluff);
    ok &= SafeRead(stats + 0x0B8, s.assassin);
    ok &= SafeRead(stats + 0x0BC, s.survival);
    ok &= SafeRead(stats + 0x0C0, s.tracking);
    ok &= SafeRead(stats + 0x0C4, s.climbing);
    ok &= SafeRead(stats + 0x0C8, s.doctor);
    ok &= SafeRead(stats + 0x0CC, s.engineer);
    ok &= SafeRead(stats + 0x0D0, s.weaponSmith);
    ok &= SafeRead(stats + 0x0D4, s.armourSmith);
    ok &= SafeRead(stats + 0x0D8, s.bowSmith);
    ok &= SafeRead(stats + 0x0DC, s.robotics);
    ok &= SafeRead(stats + 0x0E0, s.science);
    ok &= SafeRead(stats + 0x0E4, s.labouring);
    ok &= SafeRead(stats + 0x0E8, s.farming);
    ok &= SafeRead(stats + 0x0EC, s.cooking);
    ok &= SafeRead(stats + 0x0F0, s.dodging);
    ok &= SafeRead(stats + 0x0F4, s.friendlyFire);
    ok &= SafeRead(stats + 0x0F8, s.katanas);
    ok &= SafeRead(stats + 0x0FC, s.sabres);
    ok &= SafeRead(stats + 0x100, s.hackers);
    ok &= SafeRead(stats + 0x104, s.blunt);
    ok &= SafeRead(stats + 0x108, s.heavyWeapons);
    ok &= SafeRead(stats + 0x10C, s.unarmed);
    ok &= SafeRead(stats + 0x110, s.bows);
    ok &= SafeRead(stats + 0x114, s.turrets);
    ok &= SafeRead(stats + 0x118, s.polearms);
    ok &= SafeRead(stats + 0x11C, s.juryRig);
    ok &= SafeRead(stats + 0x120, s.meleeAttack);
    ok &= SafeRead(stats + 0x124, s.meleeDefence);
    return ok;
}