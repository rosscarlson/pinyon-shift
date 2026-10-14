using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace PinyonWheelSetup.Input;

/// <summary>One SDL joystick, opened raw the way the game's wheel driver opens it.</summary>
public sealed class Device : INotifyPropertyChanged
{
    public uint Id { get; init; }
    public IntPtr Handle { get; init; }
    public string Name { get; init; } = "";
    public string Path { get; init; } = "";
    public ushort Vendor { get; init; }
    public ushort Product { get; init; }
    public bool Haptic { get; init; }

    public short[] Axes { get; init; } = [];
    public bool[] Buttons { get; init; } = [];
    public byte[] Hats { get; init; } = [];
    public short[] BaseAxes { get; set; } = [];
    public bool[] BaseButtons { get; set; } = [];
    public byte[] BaseHats { get; set; } = [];
    private short[] prevAxes = [];
    private bool[] prevButtons = [];
    private byte[] prevHats = [];

    public string Ids => $"{Vendor:x4}:{Product:x4}";
    public string Summary =>
        $"{Ids} · {Axes.Length} axes · {Buttons.Length} buttons" +
        (Hats.Length > 0 ? $" · {Hats.Length} hat{(Hats.Length > 1 ? "s" : "")}" : "") +
        (Haptic ? " · force feedback" : "");

    private string activity = "";
    public string Activity { get => activity; private set { if (activity != value) { activity = value; Changed(nameof(Activity)); } } }
    private bool isActive;
    /// <summary>Moved in the last 1.5 s.</summary>
    public bool IsActive { get => isActive; private set { if (isActive != value) { isActive = value; Changed(nameof(IsActive)); } } }
    private DateTime activeUntil;

    public event PropertyChangedEventHandler? PropertyChanged;
    private void Changed(string name) => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

    public void Read()
    {
        for (int i = 0; i < Axes.Length; i++) Axes[i] = Sdl.SDL_GetJoystickAxis(Handle, i);
        for (int i = 0; i < Buttons.Length; i++) Buttons[i] = Sdl.SDL_GetJoystickButton(Handle, i);
        for (int i = 0; i < Hats.Length; i++) Hats[i] = Sdl.SDL_GetJoystickHat(Handle, i);
    }

    public void SnapshotBaseline()
    {
        BaseAxes = (short[])Axes.Clone();
        BaseButtons = (bool[])Buttons.Clone();
        BaseHats = (byte[])Hats.Clone();
        if (prevAxes.Length != Axes.Length)
        {
            prevAxes = (short[])Axes.Clone();
            prevButtons = (bool[])Buttons.Clone();
            prevHats = (byte[])Hats.Clone();
        }
    }

    public void TrackActivity(DateTime now)
    {
        for (int i = 0; i < Axes.Length; i++)
        {
            if (Math.Abs(Axes[i] - prevAxes[i]) > 2500)
            {
                Activity = $"axis {i}  {WheelMath.Signed(Axes[i])}";
                prevAxes[i] = Axes[i];
                activeUntil = now.AddSeconds(1.5);
            }
        }
        for (int i = 0; i < Buttons.Length; i++)
        {
            if (Buttons[i] != prevButtons[i])
            {
                Activity = $"button {i} {(Buttons[i] ? "down" : "up")}";
                prevButtons[i] = Buttons[i];
                activeUntil = now.AddSeconds(1.5);
            }
        }
        for (int i = 0; i < Hats.Length; i++)
        {
            if (Hats[i] != prevHats[i])
            {
                Activity = $"hat {i} {(Hats[i] == 0 ? "centre" : WheelMath.HatName(WheelMath.FirstHatBit(Hats[i])))}";
                prevHats[i] = Hats[i];
                activeUntil = now.AddSeconds(1.5);
            }
        }
        IsActive = now < activeUntil;
    }
}

/// <summary>
/// Polls SDL joysticks from the UI thread. SDL is built from the game SDK's own source with the
/// same joystick defaults, so names, paths and axis/button numbers match what the game reads.
/// </summary>
public sealed class DeviceManager : IDisposable
{
    public ObservableCollection<Device> Devices { get; } = [];
    public string? InitError { get; private set; }
    public event Action? DevicesChanged;
    private DateTime nextScan;

    public bool Start()
    {
        try
        {
            // The setup window is not an SDL window, so SDL must not treat input as unfocused.
            Sdl.SDL_SetHint("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", "1");
            if (!Sdl.SDL_Init(Sdl.InitJoystick | Sdl.InitHaptic))
            {
                InitError = "SDL could not start: " + Sdl.Error();
                return false;
            }
        }
        catch (Exception e) when (e is DllNotFoundException or EntryPointNotFoundException or BadImageFormatException)
        {
            InitError = "SDL3.dll could not be loaded: " + e.Message;
            return false;
        }
        Scan();
        return true;
    }

    /// <summary>Call ~60 times a second.</summary>
    public void Poll()
    {
        Sdl.SDL_UpdateJoysticks();
        Sdl.SDL_FlushEvents(0, uint.MaxValue);
        var now = DateTime.UtcNow;
        if (now >= nextScan) Scan();
        foreach (var d in Devices)
        {
            d.Read();
            d.TrackActivity(now);
        }
    }

    public Device? Find(uint id) => Devices.FirstOrDefault(d => d.Id == id);

    public void SnapshotBaselines()
    {
        foreach (var d in Devices) d.SnapshotBaseline();
    }

    private void Scan()
    {
        nextScan = DateTime.UtcNow.AddMilliseconds(500);
        var ids = new HashSet<uint>();
        IntPtr list = Sdl.SDL_GetJoysticks(out int count);
        if (list != IntPtr.Zero)
        {
            for (int i = 0; i < count; i++) ids.Add((uint)Marshal.ReadInt32(list, i * 4));
            Sdl.SDL_free(list);
        }
        bool changed = false;
        foreach (var gone in Devices.Where(d => !ids.Contains(d.Id)).ToList())
        {
            Sdl.SDL_CloseJoystick(gone.Handle);
            Devices.Remove(gone);
            changed = true;
        }
        foreach (uint id in ids)
        {
            if (Find(id) != null) continue;
            IntPtr js = Sdl.SDL_OpenJoystick(id);
            if (js == IntPtr.Zero) continue;
            var d = new Device
            {
                Id = id,
                Handle = js,
                Name = Sdl.Name(js),
                Path = Sdl.Path(js),
                Vendor = Sdl.SDL_GetJoystickVendor(js),
                Product = Sdl.SDL_GetJoystickProduct(js),
                Haptic = Sdl.SDL_IsJoystickHaptic(js),
                Axes = new short[Math.Max(0, Sdl.SDL_GetNumJoystickAxes(js))],
                Buttons = new bool[Math.Max(0, Sdl.SDL_GetNumJoystickButtons(js))],
                Hats = new byte[Math.Max(0, Sdl.SDL_GetNumJoystickHats(js))],
            };
            d.Read();
            d.SnapshotBaseline();
            Devices.Add(d);
            changed = true;
        }
        if (changed) DevicesChanged?.Invoke();
    }

    /// <summary>A gentle push one way then the other, to check the FFB chain (EmuWheel, vJoy, base).</summary>
    public async Task<string> TestForceAsync(Device device)
    {
        IntPtr haptic = Sdl.SDL_OpenHapticFromJoystick(device.Handle);
        if (haptic == IntPtr.Zero) return "Could not open force feedback: " + Sdl.Error();
        try
        {
            if ((Sdl.SDL_GetHapticFeatures(haptic) & Sdl.HapticConstant) == 0)
                return $"\"{device.Name}\" has no constant force; the game would only rumble it.";
            foreach (short level in new short[] { 4900, -4900 })  // 15 %
            {
                int effect = Sdl.SDL_CreateHapticEffect(haptic, Sdl.ConstantForce(level, 500));
                if (effect < 0) return "The device refused a constant force: " + Sdl.Error();
                Sdl.SDL_RunHapticEffect(haptic, effect, 1);
                await Task.Delay(650);
                Sdl.SDL_DestroyHapticEffect(haptic, effect);
            }
            return $"Sent a gentle push one way, then the other (15 %) to \"{device.Name}\".";
        }
        finally
        {
            Sdl.SDL_CloseHaptic(haptic);
        }
    }

    public void Dispose()
    {
        foreach (var d in Devices) Sdl.SDL_CloseJoystick(d.Handle);
        Devices.Clear();
        try { Sdl.SDL_Quit(); } catch (DllNotFoundException) { }
    }
}
