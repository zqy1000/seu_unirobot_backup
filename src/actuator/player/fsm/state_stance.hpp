#pragma once

#include "state.hpp"

namespace FSM
{
    class state_stance: public state
    {
    public:
        state_stance(fsm_ptr _fsm): state("stance", _fsm)
        {

        }

        std::list<plan_ptr> run(player_ptr p)
        {
            std::list< plan_ptr > plist;
            return plist;
        }
    };
}
