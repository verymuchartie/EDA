//
// Created by charlie on 9/2/26.
//


#ifndef EDA_PRIMITIVE_MODULES_H
#define EDA_PRIMITIVE_MODULES_H

#include "sim/Core.h"
#include <vector>

namespace Sim::Primitive_Modules {
    class Table_Module : public Core::ModuleTypeDef {
    public:
        /**
             * A module implementing a passed in truth table.
             * This should only be used to define simple behaviors.
             * Every possible input must be defined.
             *
             * @param name The name of this type of table modules.
             * @param input_cnt The number of inputs.
             * @param outputs A truth table of the outputs.
             * @param delay How long this module takes to update.
             */
        Table_Module(const char *name, int input_cnt, std::vector<std::vector<Core::WireState> *> *outputs,
                     int delay);

        void update(Core::Module *module, Core::EDA_Environment *env) override;

        int inputUpdateDelay() override;

    private:
        int delay;
        std::vector<std::vector<Core::WireState> *> *table;
    };

    class Log : public Core::ModuleTypeDef {
    public:
        explicit Log(const char *prefix);

        void update(Core::Module *module, Core::EDA_Environment *env) override;

        int inputUpdateDelay() override;

        static Core::ModuleId attach_logger(Core::EDA_Environment *env, Core::WireId wire_id,
                                            const char *prefix);

    private:
        const char *prefix;
    };

    Core::ModuleTypeDef *inverterTypeDef();

    Core::ModuleTypeDef *andTypeDef();
}

#endif //EDA_PRIMITIVE_MODULES_H
