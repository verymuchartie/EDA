//
// Created by charlie on 9/4/26.
//

#ifndef EDA_COMPOSITE_MODULE_H
#define EDA_COMPOSITE_MODULE_H
#include <string>

#include "Core.h"

namespace Sim::Core::Composite {
    typedef ModuleId Submodule_Id;
    typedef WireId Virtual_WireId;

    class Composite_Module_Def {
    public:
        struct Composite_Module_Wires{
            std::unordered_map<Virtual_WireId, WireId> concrete_wire_ids;
        };

        struct Composite_Module_Submodules{
            std::unordered_map<Submodule_Id, ModuleId> concrete_module_ids;
        };

        /**
         * Creates a new composite wire
         * @return The Id of the new composite wire
         */
        Virtual_WireId new_wire();

        std::vector<Virtual_WireId> new_wires(int n);

        Submodule_Id new_module(ModuleTypeId id);

        bool bind_submodule_output(Submodule_Id id, int output_index, Virtual_WireId wire_id);

        bool bind_submodule_input(Submodule_Id id, int input_index, Virtual_WireId wire_id);

        bool generate_instance(EDA_Environment *env);

        Composite_Module_Wires* generate_instance_wires(EDA_Environment *env);

        Composite_Module_Submodules* generate_instance_modules(EDA_Environment *env);

        bool bind_modules_to_wires(EDA_Environment *env, Composite_Module_Submodules* submodule, Composite_Module_Wires* wires);

        [[nodiscard]]
        const std::string *getName() const;

        ~Composite_Module_Def() = default;

    private:
        // A promise of a module.
        struct Submodule {
            ModuleTypeId module_type_id = -1;
            Submodule_Id composite_module_id = -1;
        };

        struct Virtual_Wire {
            Virtual_WireId virtual_wire_id = -1;
            std::vector<std::pair<Submodule_Id, int> > inputs;
            std::vector<std::pair<Submodule_Id, int> > outputs;
        };

        std::string name;
        int nextWireId = 4000;
        int nextSubmoduleId = 3000;
        std::vector<Submodule> submodules;
        std::vector<Virtual_Wire> virtual_wires;

        Submodule *get_submodule(Submodule_Id id);
    };
}


#endif //EDA_COMPOSITE_MODULE_H
