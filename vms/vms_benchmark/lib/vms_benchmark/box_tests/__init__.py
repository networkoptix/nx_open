class BoxTestResult:
    def __init__(self, success, message='', details=()):
        self.success = bool(success)
        self.message = str(message)
        self.details = tuple(details)

    def formatted_message(self):
        return self.message.format(*(self.details or tuple())) if self.message else ''


from .sudo_configured import SudoConfigured
