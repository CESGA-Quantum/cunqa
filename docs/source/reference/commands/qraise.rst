qraise
======

Raise one or more :term:`vQPUs <vQPU>` with a well defined configuration.

The ``qraise`` command is used to deploy a set of :term:`virtual QPUs <virtual QPU>` with a given configuration,
including computational resources, :term:`backend` selection, and communication capabilities.
The raised :term:`vQPUs <vQPU>` can subsequently be used to execute quantum circuits within the CUNQA
framework.

Synopsis
--------

.. code-block:: bash

   qraise [OPTIONS]

Options
-------

General deployment options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``-n, --num-qpus <int>``
    **Mandatory**. Number of :term:`QPUs <QPU>` to be raised.

``-t, --time <string>``
    **Mandatory**. Time during which the :term:`QPUs <QPU>` will remain active.

``-c, --cores-per-qpu <int>``
    Number of CPU cores assigned to each :term:`QPU`.
    Default: ``2``

``-p, --partition <string>``
    Partition requested for the :term:`QPUs <QPU>`.

``--mem, --mem-per-qpu <size>``
    Amount of memory assigned to each :term:`QPU`. Accepts a unit suffix ``K``, ``M``, ``G`` or ``T``
    (optionally followed by ``B``) and decimal values, e.g. ``512M``, ``1.5G`` or ``2GB``. A value
    without suffix is read as GB. The memory is computed in MB and rounded up; the minimum is ``256M``.
    Default: ``15G``

``-N, --n-nodes <int>``
    Number of compute nodes used to deploy the :term:`QPUs <QPU>`.
    Default: ``1``

``--nodelist <string>``
    List of nodes where the :term:`QPUs <QPU>` will be deployed.
    Multiple nodes can be specified at a time.

``--qpuN, --qpus-per-node <int>``
    Number of :term:`QPUs <QPU>` deployed on each node.

Backend and simulation options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``-b, --backend <string>``
    Path to the :term:`backend` configuration file.

``--sim, --simulator <string>``
    Selects :term:`simulator` responsible for running the simulations.
    Default: ``Aer``

Grouping and communication options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``--family <string>``
    Name used to identify the group of :term:`QPUs <QPU>` that were raised together.
    Default: ``default``

``--co-located``
    Enable :term:`co-located mode`.
    If set, the :term:`vQPU` can be accesed from any node.
    Otherwise, the user can only access it from the node it is deployed on.

``--classical_comm``
    Enable :term:`classical communications` between :term:`QPUs <QPU>`.

``--quantum_comm``
    Enable :term:`quantum communications` between :term:`QPUs <QPU>`.

``--executor-cores <int>``
    Only with ``--quantum_comm``. Number of cores of the shared :term:`simulator` process.
    Default: ``n * (cores-per-qpu - 1)``.

GPU execution
~~~~~~~~~~~~~~~~~~~~~~
``--gpu``
    Enable GPU execution. The quantum simulation will be performed on GPU.

``--gpu-name <string>``
    Name (type) of the GPU to request for the execution (e.g. ``a100``, ``t4``).

Real QPU
~~~~~~~~~~~~~~~~~~~~~~
``--qmio``
    *Deploys* the real quantum computer QMIO located at CESGA's installations, allowing hybrid :term:`DQC` infrastructures.


Basic usage
-----------

Command that deploys 2 :term:`vQPUs <vQPU>` with :term:`classical communications`, for 10 minutes and accessible from any node:

.. code-block:: bash

   qraise -n 2 -t 00:10:00 --classical_comm --co-located

Resources with quantum communications
-------------------------------------

With ``--quantum_comm`` the :term:`family` runs on a single node as ``n`` lightweight :term:`vQPU` servers plus
one shared :term:`simulator` process, and the job reserves exactly what these two parts need:

- **Cores**: one per server plus ``--executor-cores`` for the simulator. By default this is
  ``n * cores-per-qpu``, so ``--cores-per-qpu`` must be at least ``2`` unless ``--executor-cores`` is
  given.
- **Memory**: ``n * mem-per-qpu``. Each server takes a quarter of ``--mem-per-qpu`` (between ``128M`` and
  ``1G``) and the simulator gets the rest.

For small vQPUs the footprint can be reduced a lot. This deploys 3 vQPUs with 4 cores and 1.5 GB in 
total:

.. code-block:: bash

   qraise -n 3 -t 01:00:00 --quantum_comm --co-located --mem-per-qpu=512M --executor-cores=1

Notes
-----

- Some options are :term:`backend`- or :term:`simulator`-specific and may not be supported in all execution
  environments.
- Invalid or incompatible combinations of options may result in an error at deployment time.
