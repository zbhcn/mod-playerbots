/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license, you may redistribute it
 * and/or modify it under version 2 of the License, or (at your option), any later version.
 */

#include "GenericHunterStrategy.h"

#include "Playerbots.h"

class GenericHunterStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    GenericHunterStrategyActionNodeFactory()
    {
        creators["rapid fire"] = &rapid_fire;
        creators["boost"] = &rapid_fire;
        creators["aspect of the pack"] = &aspect_of_the_pack;
        creators["feign death"] = &feign_death;
        creators["wing clip"] = &wing_clip;
        creators["mongoose bite"] = &mongoose_bite;
        creators["raptor strike"] = &raptor_strike;
        creators["disengage"] = &disengage;
        creators["Frost Trap"] = &Frost_Trap;
        creators["explosive trap"] = &explosive_trap;
    }

private:
    static ActionNode* rapid_fire([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("rapid fire",//准备就绪
                              /*P*/ nullptr,
                              /*A*/ NextAction::array(0, new NextAction("readiness"), nullptr),
                              /*C*/ nullptr);
    }

    static ActionNode* aspect_of_the_pack([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("aspect of the pack",//豹群守护
                              /*P*/ nullptr,
                              /*A*/ NextAction::array(0, new NextAction("aspect of the cheetah"), nullptr),//猎豹守护
                              /*C*/ nullptr);
    }

    static ActionNode* feign_death([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("feign death",//假死
                              /*P*/ nullptr,
                              /*A*/ nullptr,
                              /*C*/ nullptr);
    }

    static ActionNode* wing_clip([[maybe_unused]] PlayerbotAI* botAI)
    {//摔绊
        return new ActionNode("wing clip",
                              /*P*/ nullptr,
                              /*A*/ NextAction::array(0, new NextAction("disengage", 20), nullptr),
                              /*C*/ nullptr);
    }

    static ActionNode* mongoose_bite([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("mongoose bite",
                              /*P*/ nullptr,
                              /*A*/ NextAction::array(0, new NextAction("raptor strike"), nullptr),
                              /*C*/ nullptr);
    }

    static ActionNode* raptor_strike([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("raptor strike",
                              /*A*/ NextAction::array(0, new NextAction("melee"), nullptr),
                              /*P*/ nullptr,
                              /*C*/ nullptr);
    }

    static ActionNode* disengage([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("disengage",//逃脱
                              /*P*/ nullptr,
                              /*A*/ nullptr,
                              /*C*/ nullptr);
    }

    static ActionNode* Frost_Trap([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("Frost Trap",
                              /*A*/ nullptr,
                              /*P*/ NextAction::array(0, new NextAction("melee", 28), nullptr),
                              /*C*/ nullptr);
    }

    static ActionNode* explosive_trap([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("explosive trap",
                              /*P*/ nullptr,
                              /*A*/ NextAction::array(0, new NextAction("immolation trap"), nullptr),
                              /*C*/ nullptr);
    }
};

GenericHunterStrategy::GenericHunterStrategy(PlayerbotAI* botAI) : CombatStrategy(botAI)
{
    actionNodeFactories.Add(new GenericHunterStrategyActionNodeFactory());
}

void GenericHunterStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    CombatStrategy::InitTriggers(triggers);
    //triggers.push_back(new TriggerNode("enemy too close for shoot",//重复了
    //                                   NextAction::array(0, new NextAction("disengage", ACTION_HIGH + 19), nullptr)));

    triggers.push_back(new TriggerNode("enemy within melee",
                                       NextAction::array(0, new NextAction("explosive trap", ACTION_MOVE + 7),
                                                         new NextAction("mongoose bite", ACTION_HIGH + 2),
                                                         new NextAction("wing clip", ACTION_HIGH + 1), nullptr)));
    
    triggers.push_back(new TriggerNode("enemy is close",//当距离敌人大于5码TooCloseDistance
                                       NextAction::array(0, new NextAction("rapid fire", ACTION_NORMAL + 6), nullptr)));
    triggers.push_back(new TriggerNode("medium threat",
                                       NextAction::array(0, new NextAction("feign death", ACTION_MOVE + 8), nullptr)));

    triggers.push_back(new TriggerNode("hunters pet medium health",
                                       NextAction::array(0, new NextAction("mend pet", ACTION_HIGH + 3), nullptr)));
    // triggers.push_back(new TriggerNode("no ammo", NextAction::array(0, new NextAction("switch to melee", ACTION_HIGH
    // + 1), new NextAction("say::no ammo", ACTION_HIGH), nullptr)));
    triggers.push_back(new TriggerNode("aspect of the viper",
                                       NextAction::array(0, new NextAction("aspect of the viper", ACTION_HIGH), NULL)));
    //triggers.push_back(new TriggerNode(
    //    "enemy out of melee", NextAction::array(0, new NextAction("concussive shot", ACTION_HIGH + 16), NULL)));
    triggers.push_back(new TriggerNode("enemy too close for auto shot",
                                       NextAction::array(0, new NextAction("disengage", ACTION_MOVE + 5),
                                                         new NextAction("flee", ACTION_MOVE + 4), nullptr)));
    triggers.push_back(new TriggerNode(
        "enemy player near", NextAction::array(0, new NextAction("Frost Trap", ACTION_HIGH + 15), NULL)));
    triggers.push_back(
        new TriggerNode("low tank threat", NextAction::array(0, new NextAction("misdirection on main tank", ACTION_MOVE + 1), NULL)));
    triggers.push_back(
        new TriggerNode("low health", NextAction::array(0, new NextAction("deterrence", ACTION_MOVE), nullptr)));
    
    triggers.push_back(new TriggerNode("tranquilizing shot enrage",
                                       NextAction::array(0, new NextAction("tranquilizing shot", 61.0f), NULL)));
    triggers.push_back(new TriggerNode("tranquilizing shot magic",
                                       NextAction::array(0, new NextAction("tranquilizing shot", 61.0f), NULL)));
}

NextAction** HunterBoostStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("bestial wrath", 15.0f), nullptr);
}

void HunterBoostStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode("rapid fire", NextAction::array(0, new NextAction("rapid fire", 16.0f), nullptr)));
}

void HunterCcStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode(
        "scare beast", NextAction::array(0, new NextAction("scare beast on cc", ACTION_HIGH + 3), nullptr)));
    triggers.push_back(new TriggerNode(
        "freezing trap", NextAction::array(0, new NextAction("freezing trap on cc", ACTION_HIGH + 3), nullptr)));
}

void HunterTrapWeaveStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(new TriggerNode("immolation trap no cd",
                                       NextAction::array(0, new NextAction("reach melee", ACTION_HIGH + 3), nullptr)));

    // triggers.push_back(new TriggerNode(
    //     "scare beast", NextAction::array(0, new NextAction("scare beast on cc", ACTION_HIGH + 3), nullptr)));
    // triggers.push_back(new TriggerNode(
    //     "freezing trap", NextAction::array(0, new NextAction("freezing trap on cc", ACTION_HIGH + 3), nullptr)));
}
