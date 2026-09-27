"""Unreal Editor Python — import DT_Destinations from DT_Destinations.csv.

Same shape as import_products_datatable.py, for the GDD 9.2 travel points.
Run it after the C++ has been built, so FDestinationRow is a known type.
Safe to run repeatedly.
"""

import os
import unreal

CSV_PATH = os.path.join(os.path.dirname(__file__), "DT_Destinations.csv")
DEST_PATH = "/Game/Deadline/Data"
ASSET_NAME = "DT_Destinations"
ROW_STRUCT_PATH = "/Script/DEADLINE_.DestinationRow"


def import_destinations():
    csv_path = os.path.normpath(CSV_PATH)
    if not os.path.isfile(csv_path):
        unreal.log_error("CSV not found: {0}".format(csv_path))
        return

    row_struct = unreal.load_object(None, ROW_STRUCT_PATH)
    if row_struct is None:
        unreal.log_error(
            "FDestinationRow not found ({0}). Build the C++ project first.".format(ROW_STRUCT_PATH)
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
    unreal.log("DT_Destinations imported OK: {0} rows -> {1}".format(len(row_names), full))
    for name in row_names:
        unreal.log("  row: {0}".format(name))


import_destinations()
