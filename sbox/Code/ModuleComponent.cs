using System;
using Sandbox;
public class ModuleComponent : Component
{
	private Dextr.Module Module;

	protected override void OnStart()
	{
		base.OnStart();

		Module = new();

		Module.__main_argc_argv( 0, 0 );

	}
}
