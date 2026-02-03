#include <pybind11/pybind11.h>

#include "cc/neolux/utils/unitransmit/Unitransmit.h"

namespace py = pybind11;
using cc::neolux::utils::unitransmit::UniTransmit;

namespace {

py::bytes bytes_from_vector(const std::vector<std::uint8_t> &data) {
    return py::bytes(reinterpret_cast<const char *>(data.data()), data.size());
}

py::dict context_to_dict(const UniTransmit::ReceiveContext &ctx) {
    py::dict out;
    out["ifname"] = ctx.ifname;
    out["scheme"] = ctx.scheme;
    return out;
}

} // namespace

PYBIND11_MODULE(_unitransmit, m) {
    m.doc() = "Python bindings for the unitransmit library.";

    py::class_<UniTransmit>(m, "UniTransmit")
        .def(py::init<std::string>(), py::arg("ifname"))
        .def("read", [](UniTransmit &self) { return bytes_from_vector(self.read()); })
        .def("read",
             [](UniTransmit &self, std::size_t max_bytes) {
                 return bytes_from_vector(self.read(max_bytes));
             },
             py::arg("max_bytes"))
        .def("read_all", [](UniTransmit &self) { return bytes_from_vector(self.read_all()); })
        .def("write",
             [](UniTransmit &self, py::bytes data) {
                 std::string payload = data;
                 return self.write(payload);
             },
             py::arg("data"))
        .def("write",
             [](UniTransmit &self, const std::string &data) { return self.write(data); },
             py::arg("data"))
        .def("in_waiting", &UniTransmit::in_waiting)
        .def("ifname", &UniTransmit::ifname, py::return_value_policy::reference_internal)
        .def("scheme", &UniTransmit::scheme, py::return_value_policy::reference_internal)
        .def("set_receive_callback",
             [](UniTransmit &self, py::object callback) {
                 if (callback.is_none()) {
                     self.set_receive_callback(nullptr);
                     return;
                 }
                 py::function fn = py::reinterpret_borrow<py::function>(callback);
                 self.set_receive_callback([fn](const std::vector<std::uint8_t> &data,
                                                const UniTransmit::ReceiveContext &ctx) {
                     py::gil_scoped_acquire gil;
                     try {
                         fn(bytes_from_vector(data), context_to_dict(ctx));
                     } catch (const py::error_already_set &e) {
                         py::print("unitransmit callback error:", e);
                     }
                 });
             },
             py::arg("callback"))
        .def("start", &UniTransmit::start)
        .def("close", &UniTransmit::close)
        .def("__enter__",
             [](UniTransmit &self) -> UniTransmit & {
                 self.start();
                 return self;
             },
             py::return_value_policy::reference_internal)
        .def("__exit__",
             [](UniTransmit &self, py::object, py::object, py::object) { self.close(); });
}
