"""Unreal Editor Python — import DT_Vehicles from DT_Vehicles.csv.

Same shape as import_products_datatable.py, for the GDD 9.5 fleet table.
Run it after the C++ has been built, so FVehicleRow is a known type:

    "UnrealEditor-Cmd.exe" <uproject> -run=pythonscript \
        -script="D:/DEADLINE_/Content/Deadline/Data/import_vehicles_datatable.py" \
        -unattended -nopause -nosplash -NullRHI

Safe to run repeatedly.
"""

import os
import unreal

CSV_PATH = os.path.join(os.path.dirname(__file__), "DT_Vehicles.csv")
DEST_PATH = "/Game/Deadline/Data"
ASSET_NAME = "DT_Vehicles"
ROW_STRUCT_PATH = "/Script/DEADLINE_.VehicleRow"  # FVehicleRow in module DEADLINE_


def import_vehicles():
    csv_path = os.path.normpath(CSV_PATH)
    if not os.path.isfile(csv_path):
        unreal.log_error("CSV not found: {0}".format(csv_path))
        return

    row_struct = unreal.load_object(None, ROW_STRUCT_PATH)
    if row_struct is None:
        unreal.log_error(
            "FVehicleRow not found ({0}). Build the C++ project first.".format(ROW_STRUCT_PATH)
        )
        return

    factory = unreal.CSVImportFactory()
    factory.automated_import_settings.import_row_struct = row_struct

    task = unreal.AssetImportTask()
    task.filename = csv_path
    task.destination_path = DEST_PATH
    task.destination_name = ASSET_NAME
    task.replace_existing = True
    task.automated = True
    task.save = True
    task.factory = factory

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    full = "{0}/{1}".format(DEST_PATH, ASSET_NAME)
    table = unreal.load_asset(full)
    if table is None:
        unreal.log_error("Import failed: {0}".format(full))
        return

    row_names = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    unreal.log("DT_Vehicles imported OK: {0} rows -> {1}".format(len(row_names), full))
    for name in row_names:
        unreal.log("  row: {0}".format(name))


import_vehicles()
