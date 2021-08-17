class WitcherTxCrashManager:
    def __init__(self, trace, cache, crash_validator, crash_point_generator):
        self.trace = trace
        self.cache = cache
        self.crash_validator = crash_validator
        self.crash_point_list = crash_point_generator.get_crash_point_list()

    # main logic
    def run(self):
        # init the crash_point
        crash_point_index = 0
        if crash_point_index == len(self.crash_point_list):
            return
        crash_point = self.crash_point_list[crash_point_index]
        # traverse the trace
        for op in self.trace.atomic_write_ops:
            # crash condition
            if op.id == crash_point.id:
                # flush the binary file before crash
                self.cache.binary_file.flush()
                # crash and validate
                self.crash_validator.validate(crash_point)
                # update the crash point
                crash_point_index = crash_point_index + 1
                if crash_point_index == len(self.crash_point_list):
                    break
                crash_point = self.crash_point_list[crash_point_index]
            # put the op into the cache
            self.cache.accept(op)

        # should have already tried all crash points
        assert(crash_point_index == len(self.crash_point_list))

    # for debugging the replay function
    def debug_replay(self):
        for op in self.trace.atomic_write_ops:
            self.cache.accept(op)

    # return the result
    def get_reported_crash_point_list(self):
        return self.crash_validator.get_reported_crash_point_list()

