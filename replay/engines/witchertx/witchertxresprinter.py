from engines.witcher.witcherresprinter import WitcherResPrinter

PRINT_FILE_CRASH_POINT_LIST = 'crash_point_list'
PRINT_FILE_DOUBLE_LOGGING_LIST = 'double_logging_list'
PRINT_FILE_REPORTED_CRASH_POINT_LIST = 'reported_crash_point_list'

class WitcherTxResPrinter(WitcherResPrinter):
    def __init__(self, output_dir):
        WitcherResPrinter.__init__(self, output_dir)

    def print_crash_point_list(self, crash_point_list):
        f = open(self.output_dir+'/'+PRINT_FILE_CRASH_POINT_LIST, 'w')
        for cp in crash_point_list:
            f.write(str(cp) + '\n')

    def print_double_logging_list(self, double_logging_list):
        f = open(self.output_dir+'/'+PRINT_FILE_DOUBLE_LOGGING_LIST, 'w')
        for dl in double_logging_list:
            f.write(str(dl) + '\n')

    def print_reported_crash_point_list(self, reported_crash_point_list):
        f = open(self.output_dir+'/'+PRINT_FILE_REPORTED_CRASH_POINT_LIST, 'w')
        for cp in reported_crash_point_list:
            f.write(str(cp) + '\n')
