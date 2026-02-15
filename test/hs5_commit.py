
class Hs5RestartError(Exception):
    def __str__(self) -> str:
        return "HS5 was restarted before commit. Objects might not be properly committed."

class Hs5Commit:
    def __init__(self, s3, bucket: str):
        self.s3 = s3
        self._bucket = bucket
        self._commit = False

    def __enter__(self):
        self._runtime_uuid = self._get_runtime_uuid()
        if self._runtime_uuid != "DISABLED":
            # We can skip commit if manual commit is disabled
            self._commit = True
        return self
    
    def _get_runtime_uuid(self) -> str:
        obj = self.s3.get_object(Bucket=self._bucket, Key="a711e93e-93b4-4a9e-8a0b-688797470002")
        return obj["Body"].read().decode()

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.commit()

    def commit(self):
        """
        Make sure modifications to HS5 are on disk.
        Afterwards check that HS5 has not been restarted in context.
        """
        if not self._commit:
            return
        
        self.s3.put_object(Bucket=self._bucket, Key="a711e93e-93b4-4a9e-8a0b-688797470002", Body="")

        new_runtime_uuid = self._get_runtime_uuid()
        if new_runtime_uuid != self._runtime_uuid:
            raise Hs5RestartError()

        self._commit = False
