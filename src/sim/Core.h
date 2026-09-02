//
// Created by charlie on 9/2/26.
//

#ifndef EDA_CORE_H
#define EDA_CORE_H
#include <iostream>
#include <vector>
#include <unordered_map>

namespace Sim::Core {
    typedef int WireId;
    typedef int ModuleId;
    typedef int ModuleTypeId;

    class EDA_Environment;

    enum class WireState {
        HIGH, // Logic High
        LOW, // Logic Low
        HI_Z, // Not Connected
        X, // Unknown
    };

    struct SimState {
        int currentTime;
        bool running;
    };

    struct Wire {
        WireId id{};
        WireState state{WireState::LOW};
        ModuleId setBy{-1}; // Which module last set this wire. -1 if HI_Z
        std::vector<ModuleId> dependents; // All the modules that use this wire as a input.
    };

    struct Module {
        ModuleId module_id{}; // Module Unique Id
        std::vector<WireId> inputs; // Ids of each input
        std::vector<WireId> outputs; // Ids of each output
        int next_update{}; // The time step of the update
        ModuleTypeId typeId{}; //
    };

    class ModuleTypeDef {
    public:
        int type_id{};
        int input_cnt{};
        int output_cnt{};
        std::string name;

        virtual ~ModuleTypeDef() = default;

        ModuleTypeDef() = default;

        ModuleTypeDef(int id, int input_cnt, int output_cnt, const char *name);


        virtual void update(Module *module, EDA_Environment *env);

        /**
         * Returns how long the update should take.
         * @return How long the update should take in time steps.
         */
        virtual int inputUpdateDelay();
    };

    class EDA_Environment {
    public:
        EDA_Environment() = default;

        bool verify_modules();

        bool verify_module(ModuleId module_id);

        ModuleTypeDef *getModuleTypeDef(ModuleTypeId id);

        int addModuleTypeDef(ModuleTypeDef *module);

        std::vector<WireId> makeNWires(int n);
        WireId newWire();

        bool setWire(WireId targetWire, WireState newState, ModuleId setter);

        /**
             * Configure the simulation to start running.
             * @return If the simulation successfully passed verification checks.
             */
        bool start();

        void queueModuleForUpdate(ModuleId id);

        void queueModuleForUpdate(Module *module, ModuleTypeDef *type);

        /**
         * Advances the environment one time step.
         */
        void advance();

        /**
         * Advances the environment the given number of time steps.
         * @param dt How many time steps to advance. Must be greater then 0.
         */
        void advance(int dt);


        Module *getModule(ModuleId module_id);

        Wire *get_wire(WireId wire_id);

        bool bind_module_output(ModuleId id, int output_index, WireId wire_id);

        bool bind_module_input(ModuleId module_id, int input_index, WireId wire_id);

        bool bind_module_to_module(ModuleId outputting_module_id, int output_port,
                                   ModuleId inputting_module_id, int input_port);
        [[nodiscard]]
        int getCurrentTime() const;


        /**
         * Creates a blank module instance of the given type.
         * @param typeId  Type id to generate a module of
         * @return id of the generated module
         */
        ModuleId generateModule(ModuleTypeId typeId);

    private:
        SimState currentState{.currentTime = 0, .running = false};

        int nextWire = 1;
        std::unordered_map<int, Wire *> wire_map;

        int next_module_id = 1000;
        std::unordered_map<int, Module> module_map;

        int next_moduletype_id_ = 2000;
        std::unordered_map<int, ModuleTypeDef *> deviceDef_map;
        std::vector<Module *> module_update_queue;
    };

    /**
     * Determines if giving this input to a switch results in defined behavior
     * @param state The state to test
     * @return If the state is High or Low
     */
    bool isWireStateReal(WireState state);

    /**
     * Determines if giving this input to a switch results in undefined behavior
     * @param state The state to test
     * @return If the state is not High or Low
     */
    bool isWireStateImg(WireState state);

    /**
     * Writes a human readable version of a WireState to an output stream
     * @param os Output stream to write to
     * @param wire WireState to write
     * @return The output stream
     */
    std::ostream &operator<<(std::ostream &os, WireState wire);

    /**
     * Ands two wire states together.
     * @param wire this wire
     * @param other_wire other wire
     * @return the and of these two wires
     */
    static WireState operator*(const WireState &wire, WireState other_wire);
}
#endif //EDA_CORE_H
