module mini_gcs.diagnostics;
import std;
namespace gcs {
StructuralAnalysis analyzeStructure(const Residual &residual, std::size_t count) {
    std::vector<int> parameterToRow(count, -1);
    std::function<bool(int, std::vector<bool> &)> augment = [&](int row, auto &seen) {
        for (auto p : residual.rows[row].dependencies) {
            if (p >= count)
                throw std::out_of_range("invalid equation dependency");
            if (seen[p])
                continue;
            seen[p] = true;
            if (parameterToRow[p] < 0 || augment(parameterToRow[p], seen)) {
                parameterToRow[p] = row;
                return true;
            }
        }
        return false;
    };
    StructuralAnalysis out;
    for (std::size_t r = 0; r < residual.rows.size(); ++r) {
        std::vector<bool> seen(count);
        if (augment(static_cast<int>(r), seen))
            ++out.matching;
    }
    out.dof = static_cast<int>(count) - out.matching;
    for (std::size_t i = 0; i < count; ++i)
        if (parameterToRow[i] < 0)
            out.unmatchedParameters.push_back(i);
    return out;
}
}
