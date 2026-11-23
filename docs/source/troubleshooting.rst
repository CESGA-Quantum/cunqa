Troubleshooting
===============

.. note::

   This is a starting list seeded from the error and warning messages CUNQA currently emits. It is
   meant to grow over time as new issues and resolutions are collected.

Deployment (``qraise``)
-----------------------

**"qraise needs two mandatory arguments"**
    Both the number of :term:`vQPUs <vQPU>` (``-n``/``--num-qpus``) and the maximum time (``-t``/``--time``) must
    be provided. The time follows the SLURM format ``[D-]HH:MM:SS``.

**"Simulator <name> is not available for <scheme> communications simulation"**
    The chosen ``--simulator`` does not support the requested communication scheme. Check the
    supported combinations in :doc:`reference/simulators`. Note also that the value passed to
    ``--simulator`` is the CUNQA name (e.g. ``Munich``), not the library name (MQT-DDSIM).

**"There are QPUs with the same family name as the provided"**
    A group of :term:`vQPUs <vQPU>` with that ``--family`` name is already deployed. Choose a different :term:`family` name,
    or release the existing one with ``qdrop --family <name>`` first.

**"At this moment, only Aer supports GPU simulation"**
    GPU execution (``--gpu``/``--gpu-name``) is only available with ``--simulator Aer``, and requires
    a GPU-enabled build (``-DUSE_GPU=ON``); see :doc:`installation/getting_started`.

**"With --quantum_comm, cores_per_qpu must be equal or greater than 2"**
    By default, the quantum-communications scheme gives one core to each :term:`vQPU` server and the rest
    (``n * (cores-per-qpu - 1)``) to the shared :term:`simulator` process, so ``--cores-per-qpu`` must be at
    least ``2``. Alternatively, set the simulator cores explicitly with ``--executor-cores``; see :doc:`reference/commands/qraise`.

**"Invalid --mem-per-qpu value" / "--mem-per-qpu must be at least 256M per vQPU"**
    ``--mem-per-qpu`` takes a number with an optional unit suffix ``K``, ``M``, ``G`` or ``T``
    (``512M``, ``1.5G``, ``2GB``); without suffix it is read as GB. Each :term:`vQPU` needs at least ``256M``.

**"--quantum_comm families run on a single node"**
    A quantum-communications :term:`family` is deployed on a single node, so ``--n-nodes`` greater than 1 and
    ``--qpus-per-node`` cannot be combined with ``--quantum_comm``.

**"sbatch submission failed" (Python** :py:func:`~cunqa.qpu.qraise` **)**
    The underlying ``sbatch`` call returned an error. Inspect the reported ``stderr`` — common causes
    are an invalid partition, requesting more resources than available, or a malformed time string.

Querying / releasing (``qinfo`` / ``qdrop``)
--------------------------------------------

**"There are not deployed QPUs!"**
    No :term:`vQPUs <vQPU>` are currently registered. Deploy some with ``qraise`` first. The registry
    lives under ``$STORE/.cunqa/qpus/``.

**"Problem accessing to the QPUs on the current node. Probably the command was run on a login node"**
    ``qinfo --mynode`` relies on the ``SLURMD_NODENAME`` variable, which is only set on compute
    nodes. Run it from within an allocation, or query a specific node with ``qinfo <node>``.

**"No qraise jobs are currently running with the specified id / family names"**
    The given IDs or :term:`family` names did not match any deployed :term:`vQPU`. Use ``qinfo`` to list what is
    currently deployed.

**"You must specify either the IDs or the family name (with --family) ... or use the --all flag"**
    ``qdrop`` needs exactly one target selector: job IDs, ``--family``, or ``--all``. Combining IDs
    with ``--family`` is not allowed.

Execution (:py:func:`~cunqa.qpu.run`)
-------------------------------------

**"No QPUs were provided"**
    :py:func:`~cunqa.qpu.run` was called with ``qpus=None``. This usually means
    :py:func:`~cunqa.qpu.get_QPUs` returned nothing — see the next entries.

**"There are not enough QPUs"**
    More circuits than :term:`vQPUs <vQPU>` were submitted. Provide at least as many :term:`vQPUs <vQPU>` as circuits (a single
    circuit may be broadcast to several :term:`vQPUs <vQPU>`, but not the other way around).

**"Not enough data qubits / comm qubits in the QPU for the circuit"**
    The circuit needs more (data or communication) qubits than the target :term:`vQPU`'s :term:`backend` provides.
    Reduce the circuit size or deploy :term:`vQPUs <vQPU>` with a larger :term:`backend`.

**"You are searching for QPUs in a login node, none are found" (warning)**
    :py:func:`~cunqa.qpu.get_QPUs` was called without ``co_located=True`` from a login node. Use
    ``co_located=True`` if the :term:`vQPUs <vQPU>` were deployed in :term:`co-located mode`, or run from a compute node.

**"No QPUs were found" / "No QPUs were found with the characteristics provided" (warning)**
    No registered :term:`vQPU` matched the requested ``co_located`` / ``family`` filters. Verify the :term:`family`
    name and the access mode used at deployment with ``qinfo``.
