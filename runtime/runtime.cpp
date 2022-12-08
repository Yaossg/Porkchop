#include "runtime.hpp"

namespace Porkchop {

size_t Runtime::Func::call(Assembly *assembly) const try {
    auto& f = assembly->functions[func];
    if (std::holds_alternative<Instructions>(f)) {
        auto& instructions = std::get<Instructions>(f);
        Runtime runtime(assembly, captures);
        for (size_t i = 0; i < instructions.size(); ++i) {
            auto&& [opcode, args] = instructions[i];
            switch (opcode) {
                case Opcode::NOP:
                    break;
                case Opcode::DUP:
                    runtime.dup();
                    break;
                case Opcode::POP:
                    runtime.pop();
                    break;
                case Opcode::JMP:
                    i = std::get<size_t>(args) - 1;
                    break;
                case Opcode::JMP0:
                    if (!runtime.ipop()) {
                        i = std::get<size_t>(args) - 1;
                    }
                    break;
                case Opcode::CONST:
                    runtime.const_(std::get<size_t>(args));
                    break;
                case Opcode::STRING:
                    runtime.string(std::get<std::string>(args));
                    break;
                case Opcode::FUNC:
                    runtime.func(std::get<size_t>(args));
                    break;
                case Opcode::LOAD:
                    runtime.load(std::get<size_t>(args));
                    break;
                case Opcode::STORE:
                    runtime.store(std::get<size_t>(args));
                    break;
                case Opcode::TLOAD:
                    runtime.tload(std::get<size_t>(args));
                    break;
                case Opcode::LLOAD:
                    runtime.lload();
                    break;
                case Opcode::DLOAD:
                    runtime.dload();
                    break;
                case Opcode::TSTORE:
                    runtime.tstore(std::get<size_t>(args));
                    break;
                case Opcode::LSTORE:
                    runtime.lstore();
                    break;
                case Opcode::DSTORE:
                    runtime.dstore();
                    break;
                case Opcode::CALL:
                    runtime.call();
                    break;
                case Opcode::BIND:
                    runtime.bind(std::get<size_t>(args));
                    break;
                case Opcode::LOCAL:
                    runtime.local(std::get<size_t>(args));
                    break;
                case Opcode::AS:
                    runtime.as(std::get<TypeReference>(args));
                    break;
                case Opcode::IS:
                    runtime.is(std::get<TypeReference>(args));
                    break;
                case Opcode::ANY:
                    runtime.any(std::get<TypeReference>(args));
                    break;
                case Opcode::I2B:
                    runtime.i2b();
                    break;
                case Opcode::I2C:
                    runtime.i2c();
                    break;
                case Opcode::I2F:
                    runtime.i2f();
                    break;
                case Opcode::F2I:
                    runtime.f2i();
                    break;
                case Opcode::TUPLE:
                    runtime.tuple(std::get<size_t>(args));
                    break;
                case Opcode::LIST:
                    runtime.list(std::get<size_t>(args));
                    break;
                case Opcode::SET:
                    runtime.set(std::get<size_t>(args));
                    break;
                case Opcode::DICT:
                    runtime.dict(std::get<size_t>(args));
                    break;
                case Opcode::RETURN:
                    return runtime.return_();
                case Opcode::INEG:
                    runtime.ineg();
                    break;
                case Opcode::FNEG:
                    runtime.fneg();
                    break;
                case Opcode::NOT:
                    runtime.not_();
                    break;
                case Opcode::INV:
                    runtime.inv();
                    break;
                case Opcode::OR:
                    runtime.or_();
                    break;
                case Opcode::XOR:
                    runtime.xor_();
                    break;
                case Opcode::AND:
                    runtime.and_();
                    break;
                case Opcode::SHL:
                    runtime.shl();
                    break;
                case Opcode::SHR:
                    runtime.shr();
                    break;
                case Opcode::USHR:
                    runtime.ushr();
                    break;
                case Opcode::UCMP:
                    runtime.ucmp();
                    break;
                case Opcode::ICMP:
                    runtime.icmp();
                    break;
                case Opcode::FCMP:
                    runtime.fcmp();
                    break;
                case Opcode::SCMP:
                    runtime.scmp();
                    break;
                case Opcode::EQ:
                    runtime.eq();
                    break;
                case Opcode::NE:
                    runtime.ne();
                    break;
                case Opcode::LT:
                    runtime.lt();
                    break;
                case Opcode::GT:
                    runtime.gt();
                    break;
                case Opcode::LE:
                    runtime.le();
                    break;
                case Opcode::GE:
                    runtime.ge();
                    break;
                case Opcode::SADD:
                    runtime.sadd();
                    break;
                case Opcode::IADD:
                    runtime.iadd();
                    break;
                case Opcode::FADD:
                    runtime.fadd();
                    break;
                case Opcode::ISUB:
                    runtime.isub();
                    break;
                case Opcode::FSUB:
                    runtime.fsub();
                    break;
                case Opcode::IMUL:
                    runtime.imul();
                    break;
                case Opcode::FMUL:
                    runtime.fmul();
                    break;
                case Opcode::IDIV:
                    runtime.idiv();
                    break;
                case Opcode::FDIV:
                    runtime.fdiv();
                    break;
                case Opcode::IREM:
                    runtime.irem();
                    break;
                case Opcode::FREM:
                    runtime.frem();
                    break;
                case Opcode::INC:
                    runtime.inc(std::get<size_t>(args));
                    break;
                case Opcode::DEC:
                    runtime.dec(std::get<size_t>(args));
                    break;
                case Opcode::ITER:
                    runtime.iter(std::get<size_t>(args));
                    break;
                case Opcode::PEEK:
                    runtime.peek();
                    break;
                case Opcode::NEXT:
                    runtime.next();
                    break;
            }
        }
        __builtin_unreachable();
    } else {
        return std::get<ExternalFunctionR>(f)(captures);
    }
} catch (Runtime::Exception& e) {
    e.append("at func " + std::to_string(func));
    throw;
}

}