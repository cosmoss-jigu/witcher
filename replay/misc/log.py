import logging

def setup_logger():
    #logging.basicConfig(filename='example.log')

    formatter = logging.Formatter(fmt='%(levelname)s - %(module)s - %(message)s')

    handler = logging.StreamHandler()
    handler.setFormatter(formatter)

    logger = logging.getLogger()
    #logger.setLevel(logging.DEBUG)
    logger.setLevel(logging.ERROR)
    logger.addHandler(handler)
    return logger
