# Model Files

`phi3.gguf` is split into LFS-tracked parts because GitHub rejected the
single 2.39 GB model object. Rebuild it locally before running the app:

```powershell
powershell -ExecutionPolicy Bypass -File .\models\reassemble_phi3.ps1
```

Expected SHA256:

```text
8A83C7FB9049A9B2E92266FA7AD04933BB53AA1E85136B7B30F1B8000FF2EDEF
```
