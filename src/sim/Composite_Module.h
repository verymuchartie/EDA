//
// Created by charlie on 9/4/26.
//

#ifndef EDA_COMPOSITE_MODULE_H
#define EDA_COMPOSITE_MODULE_H
#include <string>

#include "Core.h"

namespace Sim::Core::Composite {
    typedef ModuleId Submodule_Id;
    typedef WireId Composite_WireId;

    class Composite_Module {
    public:
        /**
         * Creates a new composite wire
         * @return The Id of the new composite wire
         */
        Composite_WireId new_wire();

        std::vector<Composite_WireId> new_wires(int n);

        Submodule_Id new_module(ModuleTypeId id);

        bool bind_composite_module_output(Submodule_Id id, int output_index, Composite_WireId wire_id);

        bool bind_composite_module_input(Submodule_Id id, int input_index, Composite_WireId wire_id);

        bool generate_instance(EDA_Environment *env);

        [[nodiscard]]
        const std::string *getName() const;

        ~Composite_Module() = default;

    private:
        // A promise of a module.
        struct Submodule {
            ModuleTypeId module_type_id = -1;
            Submodule_Id composite_module_id = -1;
            ModuleId concrete_module_id = -1; // This is only set during instantiation in a environment
        };

        struct Composite_Wire {
            Composite_WireId com_wire_id = -1;
            std::vector<std::pair<Submodule_Id, int> > inputs;
            std::vector<std::pair<Submodule_Id, int> > outputs;
            WireId concrete_wire_id; // This is only set during instantiation in a environment
        };

        std::string name;
        int nextWireId = 4000;
        int nextSubmoduleId = 3000;
        std::vector<Submodule> submodules;
        std::vector<Composite_Wire> composite_wires;

        Submodule *get_submodule(Submodule_Id id);
    };
}


#endif //EDA_COMPOSITE_MODULE_H
