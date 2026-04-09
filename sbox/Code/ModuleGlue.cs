using Sandbox;

namespace Dextr;

// Glue some stuff into our modules for networking.
[SkipCodeGen]
[SkipHotload]
internal partial class Module
{
	public void Start()
	{
		using var scope = new StringScope( "OpenTTD" );
		int sp = 0;
		int argc = 1;
		int argv = Mem.StackAlloc( argc * 4, ref sp, 4 );
		Mem.Store( argv, scope.Pointer );

		__main_argc_argv( argc, argv );

		Mem.PopStack( 4 );
	}

    public static void Bootstrap(int downloadedBytes, int totalBytes)
    {
        Log.Info($"Bootstrap {downloadedBytes}/{totalBytes}.");
    }

    public static void BootstrapFailed()
    {
        Log.Error("Bootstrap failed!");

    }
}
