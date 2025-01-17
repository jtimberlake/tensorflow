/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
// Python bindings for tensorflow/python/framework/python_api_dispatcher.h.

#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11/stl.h"
#include "tensorflow/python/framework/python_api_dispatcher.h"
#include "tensorflow/python/lib/core/safe_pyobject_ptr.h"

namespace py = pybind11;

using tensorflow::py_dispatch::PyInstanceChecker;
using tensorflow::py_dispatch::PyListChecker;
using tensorflow::py_dispatch::PySignatureChecker;
using tensorflow::py_dispatch::PythonAPIDispatcher;
using tensorflow::py_dispatch::PyTypeChecker;
using tensorflow::py_dispatch::PyUnionChecker;

namespace {

py::handle Dispatch(PythonAPIDispatcher* self, py::handle args,
                    py::handle kwargs) {
  auto result = self->Dispatch(args.ptr(), kwargs.ptr());
  if (result == nullptr) {
    throw py::error_already_set();
  } else {
    return result.release();
  }
}

PythonAPIDispatcher MakePythonAPIDispatcher(
    const std::string& api_name, const std::vector<std::string>& arg_names,
    py::handle defaults) {
  std::vector<const char*> name_strs;
  name_strs.reserve(arg_names.size());
  for (const auto& name : arg_names) {
    name_strs.push_back(name.c_str());
  }
  absl::Span<const char*> arg_names_span(name_strs);
  if (defaults.ptr() == Py_None) {
    return PythonAPIDispatcher(api_name, arg_names_span, {});
  } else {
    tensorflow::Safe_PyObjectPtr fast_defaults(
        PySequence_Fast(defaults.ptr(), "defaults is not a sequence"));
    if (!fast_defaults) {
      throw py::error_already_set();
    }
    return PythonAPIDispatcher(
        api_name, arg_names_span,
        absl::MakeSpan(PySequence_Fast_ITEMS(fast_defaults.get()),
                       PySequence_Fast_GET_SIZE(fast_defaults.get())));
  }
}

}  // namespace

PYBIND11_MODULE(_pywrap_python_api_dispatcher, m) {
  py::enum_<PyTypeChecker::MatchType>(m, "MatchType")
      .value("NO_MATCH", PyTypeChecker::MatchType::NO_MATCH)
      .value("MATCH", PyTypeChecker::MatchType::MATCH)
      .value("MATCH_COMPOSITE", PyTypeChecker::MatchType::MATCH_COMPOSITE)
      .export_values();

  py::class_<PyTypeChecker, std::shared_ptr<PyTypeChecker>>(m, "PyTypeChecker")
      .def("Check", [](PyTypeChecker* self,
                       py::handle value) { return self->Check(value.ptr()); })
      .def("cost", &PyTypeChecker::cost)
      .def("cache_size",
           [](PyTypeChecker* self) {
             return dynamic_cast<PyInstanceChecker*>(self)->cache_size();
           })
      .def("__repr__", [](PyTypeChecker* self) {
        return absl::StrCat("<PyTypeChecker ", self->DebugString(), ">");
      });

  py::class_<PySignatureChecker>(m, "PySignatureChecker")
      .def(py::init<
           std::vector<std::pair<int, std::shared_ptr<PyTypeChecker>>>>())
      .def("CheckCanonicalizedArgs",
           [](PySignatureChecker* self, py::tuple args) {
             tensorflow::Safe_PyObjectPtr seq(PySequence_Fast(args.ptr(), ""));
             PyObject** items = PySequence_Fast_ITEMS(seq.get());
             int n = PySequence_Fast_GET_SIZE(seq.get());
             return self->CheckCanonicalizedArgs(absl::MakeSpan(items, n));
           })
      .def("__repr__", [](PySignatureChecker* self) {
        return absl::StrCat("<PySignatureChecker ", self->DebugString(), ">");
      });

  py::class_<PythonAPIDispatcher>(m, "PythonAPIDispatcher")
      .def(py::init(&MakePythonAPIDispatcher))
      .def("Register",
           [](PythonAPIDispatcher* self, PySignatureChecker signature_checker,
              py::handle func) {
             return self->Register(signature_checker, func.ptr());
           })
      .def("Dispatch", &Dispatch)
      .def("Unregister",
           [](PythonAPIDispatcher* self, py::handle func) {
             return self->Unregister(func.ptr());
           })
      .def("__repr__", &PythonAPIDispatcher::DebugString);

  m.def("MakeInstanceChecker", [](py::handle py_class) {
    return std::shared_ptr<PyTypeChecker>(
        std::make_shared<PyInstanceChecker>(py_class.ptr()));
  });
  m.def("MakeListChecker", [](std::shared_ptr<PyTypeChecker> elt_type) {
    return std::shared_ptr<PyTypeChecker>(
        std::make_shared<PyListChecker>(elt_type));
  });
  m.def("MakeUnionChecker",
        [](const std::vector<std::shared_ptr<PyTypeChecker>>& options) {
          return std::shared_ptr<PyTypeChecker>(
              std::make_shared<PyUnionChecker>(options));
        });
}
