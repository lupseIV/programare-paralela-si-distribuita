#include "Config.h"

#include <stdexcept>

#include "LockStrategy.h"

std::string toString(Granularity g) {
    switch (g) {
        case Granularity::PerWarehouse: return "per-warehouse";
        case Granularity::PerProduct:   return "per-product";
        case Granularity::PerCell:      return "per-cell";
    }
    return "?";
}

std::string toString(ExecutionMode m) {
    switch (m) {
        case ExecutionMode::Sequential:       return "sequential";
        case ExecutionMode::ListPerThread:    return "list-per-thread";
        case ExecutionMode::OneMovePerThread: return "one-move-per-thread";
    }
    return "?";
}

std::unique_ptr<LockStrategy> makeLockStrategy(Granularity g, std::uint32_t warehouses,
                                               std::uint32_t products) {
    switch (g) {
        case Granularity::PerWarehouse: return std::make_unique<PerWarehouseLocks>(warehouses);
        case Granularity::PerProduct:   return std::make_unique<PerProductLocks>(products);
        case Granularity::PerCell:      return std::make_unique<PerCellLocks>(warehouses, products);
    }
    throw std::invalid_argument("unknown Granularity");
}
