@rem profile: Run the profiling benchmark, varying the number of threads
@rem Requires that VorteGridPTbb be in the parent subdirectory.

copy ..\VorteGrid\ProfileWithTbb\VorteGrid.exe VorteGridPTbb.exe
set NUMBER_OF_PROCESSORS_ORIG=%NUMBER_OF_PROCESSORS%
set NUMBER_OF_PROCESSORS=1
VorteGridPTbb
set NUMBER_OF_PROCESSORS=2
VorteGridPTbb
set NUMBER_OF_PROCESSORS=3
VorteGridPTbb
set NUMBER_OF_PROCESSORS=4
VorteGridPTbb
set NUMBER_OF_PROCESSORS=6
VorteGridPTbb
set NUMBER_OF_PROCESSORS=8
VorteGridPTbb
set NUMBER_OF_PROCESSORS=12
VorteGridPTbb
set NUMBER_OF_PROCESSORS=16
VorteGridPTbb
set NUMBER_OF_PROCESSORS=%NUMBER_OF_PROCESSORS_ORIG%

pause