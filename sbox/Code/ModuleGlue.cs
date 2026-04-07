using Sandbox;

namespace Dextr;

// Glue some stuff into our modules for networking.
[SkipCodeGen]
[SkipHotload]
internal partial class Module
{
    public static void Bootstrap(int downloadedBytes, int totalBytes)
    {
        Log.Info($"Bootstrap {downloadedBytes}/{totalBytes}.");
    }

    public static void BootstrapFailed()
    {
        Log.Error("Bootstrap failed!");
    }
}
