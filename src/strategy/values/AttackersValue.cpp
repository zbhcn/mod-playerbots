/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "AttackersValue.h"

#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Playerbots.h"
#include "ReputationMgr.h"
#include "ServerFacade.h"

GuidVector AttackersValue::Calculate()
{
    std::unordered_set<Unit*> targets;

    GuidVector result;
    if (!botAI->AllowActivity(ALL_ACTIVITY))
        return result;

    AddAttackersOf(bot, targets);

    if (Group* group = bot->GetGroup())
        AddAttackersOf(group, targets);

    RemoveNonThreating(targets);

    // prioritized target
    GuidVector prioritizedTargets = AI_VALUE(GuidVector, "prioritized targets");
    for (ObjectGuid target : prioritizedTargets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (unit && IsValidTarget(unit, bot))
        {
            targets.insert(unit);
        }
    }
    if (Group* group = bot->GetGroup())
    {
        ObjectGuid skullGuid = group->GetTargetIcon(7);
        Unit* skullTarget = botAI->GetUnit(skullGuid);
        if (skullTarget && IsValidTarget(skullTarget, bot))
        {
            targets.insert(skullTarget);
        }
    }

    for (Unit* unit : targets)
        result.push_back(unit->GetGUID());

    if (bot->duel && bot->duel->Opponent)
        result.push_back(bot->duel->Opponent->GetGUID());

    // workaround for bots of same faction not fighting in arena
    //if (bot->InArena())
    //{
    //    GuidVector possibleTargets = AI_VALUE(GuidVector, "possible targets");
    //    for (ObjectGuid const guid : possibleTargets)
    //    {
    //        Unit* unit = botAI->GetUnit(guid);
    //        if (unit && unit->IsPlayer() && IsValidTarget(unit, bot))
    //        {
    //            result.push_back(unit->GetGUID());
    //        }
    //    }
    //}
    // 修改后的竞技场目标逻辑：按优先级攻击敌方目标
    if (bot->InBattleground() || bot->InArena())
    {
        GuidVector possibleTargets = AI_VALUE(GuidVector, "possible targets");
        std::map<int, GuidVector> priorityTargets;  // 按优先级存储目标

        // 遍历所有可能的目标
        for (ObjectGuid const& guid : possibleTargets)
        {
            if (Unit* unit = botAI->GetUnit(guid))
            {
                if (unit->IsPlayer() && IsValidTarget(unit, bot) && bot->IsHostileTo(unit))
                {
                    int priority = 6;  // 默认优先级为6（其他职业）
                    switch (unit->getClass())
                    {
                        case CLASS_HUNTER:
                            priority = 1;
                            break;  // 猎人优先级为1
                        case CLASS_PRIEST:
                            priority = 2;
                            break;  // 牧师优先级为2
                        case CLASS_ROGUE:
                            priority = 3;
                            break;  // 盗贼优先级为3
                        case CLASS_MAGE:
                            priority = 4;
                            break;  // 法师优先级为4
                        case CLASS_WARLOCK:
                            priority = 5;
                            break;  // 术士优先级为5
                        default:
                            priority = 6;
                            break;  // 其他职业优先级为6
                    }
                    priorityTargets[priority].push_back(unit->GetGUID());  // 按优先级分类存储
                }
            }
        }

        // 按优先级顺序将目标加入结果列表
        for (int priority = 1; priority <= 6; ++priority)
        {
            if (priorityTargets.find(priority) != priorityTargets.end())
            {
                result.insert(result.end(), priorityTargets[priority].begin(), priorityTargets[priority].end());
            }
        }
        //botAI->GetAiObjectContext()->GetValue<GuidVector>("prioritized targets")->Set({result});//设置为优先目标
    }

    return result;
}

void AttackersValue::AddAttackersOf(Group* group, std::unordered_set<Unit*>& targets)
{
    Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); itr++)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (!member || !member->IsAlive() || member == bot || member->GetMapId() != bot->GetMapId() ||
            sServerFacade->GetDistance2d(bot, member) > sPlayerbotAIConfig->sightDistance)
            continue;

        AddAttackersOf(member, targets);
    }
}

struct AddGuardiansHelper
{
    explicit AddGuardiansHelper(std::vector<Unit*>& units) : units(units) {}

    void operator()(Unit* target) const { units.push_back(target); }

    std::vector<Unit*>& units;
};

void AttackersValue::AddAttackersOf(Player* player, std::unordered_set<Unit*>& targets)
{
    if (!player || !player->IsInWorld() || player->IsBeingTeleported())
        return;

    HostileRefMgr& refManager = player->getHostileRefMgr();
    HostileReference* ref = refManager.getFirst();
    if (!ref)
        return;

    while (ref)
    {
        ThreatMgr* threatMgr = ref->GetSource();
        Unit* attacker = threatMgr->GetOwner();
        Unit* victim = attacker->GetVictim();

        if (player->IsValidAttackTarget(attacker) &&
            player->GetDistance2d(attacker) < sPlayerbotAIConfig->sightDistance)
        {
            targets.insert(attacker);
        }
        ref = ref->next();
    }
}

void AttackersValue::RemoveNonThreating(std::unordered_set<Unit*>& targets)
{
    for (std::unordered_set<Unit*>::iterator tIter = targets.begin(); tIter != targets.end();)
    {
        Unit* unit = *tIter;
        if (bot->GetMapId() != unit->GetMapId() || !hasRealThreat(unit) || !IsValidTarget(unit, bot))
        {
            std::unordered_set<Unit*>::iterator tIter2 = tIter;
            ++tIter;
            targets.erase(tIter2);
        }
        else
            ++tIter;
    }
}

bool AttackersValue::hasRealThreat(Unit* attacker)
{
    return attacker && attacker->IsInWorld() && attacker->IsAlive() && !attacker->IsPolymorphed() &&
           // !attacker->isInRoots() &&
           !attacker->IsFriendlyTo(bot);
    (attacker->GetThreatMgr().getCurrentVictim() || dynamic_cast<Player*>(attacker));
}

bool AttackersValue::IsPossibleTarget(Unit* attacker, Player* bot, float range)
{
    Creature* c = attacker->ToCreature();
    bool rti = false;
    if (attacker && bot->GetGroup())
        rti = bot->GetGroup()->GetTargetIcon(7) == attacker->GetGUID();

    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);

    bool leaderHasThreat = false;
    if (attacker && bot->GetGroup() && botAI->GetMaster())
        leaderHasThreat = attacker->GetThreatMgr().GetThreat(botAI->GetMaster());

    bool isMemberBotGroup = false;
    if (bot->GetGroup() && botAI->GetMaster())
    {
        PlayerbotAI* masterBotAI = GET_PLAYERBOT_AI(botAI->GetMaster());
        if (masterBotAI && !masterBotAI->IsRealPlayer())
            isMemberBotGroup = true;
    }
    // bool inCannon = botAI->IsInVehicle(false, true);
    // bool enemy = botAI->GetAiObjectContext()->GetValue<Unit*>("enemy player target")->Get();
    // 在竞技场中，忽略宠物目标
    //if (attacker && (bot->InBattleground() || bot->InArena()) && attacker->IsPet())
    //    return false;

    return attacker && attacker->IsVisible() && attacker->IsInWorld() && attacker->GetMapId() == bot->GetMapId() &&
           !attacker->isDead() &&
           !attacker->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NON_ATTACKABLE_2) &&
           // (inCannon || !attacker->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE)) &&
           //attacker->CanSeeOrDetect(bot) &&
           // !(attacker->HasUnitState(UNIT_STATE_STUNNED) && botAI->HasAura("shackle undead", attacker)) &&
           // !((attacker->IsPolymorphed() || botAI->HasAura("sap", attacker) || /*attacker->IsCharmed() ||*/
           // attacker->isFeared()) && !rti) &&
           /*!sServerFacade->IsInRoots(attacker) &&*/
           !attacker->IsFriendlyTo(bot) && !attacker->HasSpiritOfRedemptionAura() &&
           // !(attacker->GetGUID().IsPet() && enemy) &&
           !(attacker->GetCreatureType() == CREATURE_TYPE_CRITTER && !attacker->IsInCombat()) &&
           !attacker->HasUnitFlag(UNIT_FLAG_IMMUNE_TO_PC) && !attacker->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) &&
           // 为了战场中禁用视野检查
           (bot->InArena() || bot->CanSeeOrDetect(attacker)) &&
           !(sPlayerbotAIConfig->IsPvpProhibited(attacker->GetZoneId(), attacker->GetAreaId()) &&
             (attacker->GetGUID().IsPlayer() || attacker->GetGUID().IsPet())) &&
           !(attacker->IsPlayer() && !attacker->IsPvP() && !attacker->IsFFAPvP() &&
             (!bot->duel || bot->duel->Opponent != attacker)) &&
           (!c ||
            (!c->IsInEvadeMode() &&
             ((!isMemberBotGroup && botAI->HasStrategy("attack tagged", BOT_STATE_NON_COMBAT)) || leaderHasThreat ||
              (!c->hasLootRecipient() &&
               (!c->GetVictim() ||
                (c->GetVictim() &&
                 ((!c->GetVictim()->IsPlayer() || bot->IsInSameGroupWith(c->GetVictim()->ToPlayer())) ||
                  (botAI->GetMaster() && c->GetVictim() == botAI->GetMaster()))))) ||
              c->isTappedBy(bot))));
}

bool AttackersValue::IsValidTarget(Unit* attacker, Player* bot)
{
    //竞技场禁用视野检查
    return IsPossibleTarget(attacker, bot) && (bot->InArena() || bot->IsWithinLOSInMap(attacker));
    // (attacker->GetThreatMgr().getCurrentVictim() || attacker->GetGuidValue(UNIT_FIELD_TARGET) ||
    // attacker->GetGUID().IsPlayer() || attacker->GetGUID() ==
    // GET_PLAYERBOT_AI(bot)->GetAiObjectContext()->GetValue<ObjectGuid>("pull target")->Get());
}

bool PossibleAddsValue::Calculate()
{
    GuidVector possible = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
    GuidVector attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();

    for (ObjectGuid const guid : possible)
    {
        if (find(attackers.begin(), attackers.end(), guid) != attackers.end())
            continue;

        if (Unit* add = botAI->GetUnit(guid))
        {
            // 在竞技场中，忽略宠物目标
            //if ((bot->InBattleground() || bot->InArena()) && add->IsPet())
            //    continue;

            if (!add->GetTarget() && !add->GetThreatMgr().getCurrentVictim() && add->IsHostileTo(bot))
            {
                for (ObjectGuid const attackerGUID : attackers)
                {
                    Unit* attacker = botAI->GetUnit(attackerGUID);
                    if (!attacker)
                        continue;

                    float dist = sServerFacade->GetDistance2d(attacker, add);
                    if (sServerFacade->IsDistanceLessOrEqualThan(dist, sPlayerbotAIConfig->aoeRadius * 1.5f))
                        continue;

                    if (sServerFacade->IsDistanceLessOrEqualThan(dist, sPlayerbotAIConfig->aggroDistance))
                        return true;
                }
            }
        }
    }

    return false;
}
