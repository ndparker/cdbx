/*
 * Copyright 2016 - 2024
 * Andr\xe9 Malo or his licensors, as applicable
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cdbx.h"

EXT_INIT_FUNC;

/* ------------------------ BEGIN Helper Functions ----------------------- */

/* ------------------------- END Helper Functions ------------------------ */

/* ----------------------- BEGIN MODULE DEFINITION ----------------------- */

EXT_METHODS = {
    {NULL}  /* Sentinel */
};

PyDoc_STRVAR(EXT_DOCS_VAR,
":Copyright:\n\
\n\
 Copyright 2016 - 2024\n\
 Andr\xc3\xa9 Malo or his licensors, as applicable\n\
\n\
:License:\n\
\n\
 Licensed under the Apache License, Version 2.0 (the \"License\");\n\
 you may not use this file except in compliance with the License.\n\
 You may obtain a copy of the License at\n\
\n\
     http://www.apache.org/licenses/LICENSE-2.0\n\
\n\
 Unless required by applicable law or agreed to in writing, software\n\
 distributed under the License is distributed on an \"AS IS\" BASIS,\n\
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n\
 See the License for the specific language governing permissions and\n\
 limitations under the License.\n\
\n\
========================================\n\
 cdbx - CDB Reimplementation for Python\n\
========================================\n\
\n\
cdbx - CDB Reimplementation for Python");

EXT_DEFINE(EXT_MODULE_NAME, EXT_METHODS_VAR, EXT_DOCS_VAR);

EXT_INIT_FUNC {
    PyObject *m;

    /* Create the module and populate stuff */
    if (!(m = EXT_CREATE(&EXT_DEFINE_VAR)))
        EXT_INIT_ERROR(LCOV_EXCL_LINE(NULL));

    EXT_DOC_UNICODE(m);

    EXT_ADD_UNICODE(m, "__author__", "Andr\xe9 Malo", "latin-1");
    EXT_ADD_UNICODE(m, "__license__", "Apache License, Version 2.0", "ascii");
    EXT_ADD_STRING(m, "__version__", STRINGIFY(EXT_VERSION));

    EXT_INIT_TYPE(m, &CDBType);
    EXT_ADD_TYPE(m, "CDB", &CDBType);
    EXT_INIT_TYPE(m, &CDBIterType);
    EXT_INIT_TYPE(m, &CDBMakerType);
    EXT_ADD_TYPE(m, "CDBMaker", &CDBMakerType);

    EXT_INIT_RETURN(m);
}

/* ------------------------ END MODULE DEFINITION ------------------------ */
