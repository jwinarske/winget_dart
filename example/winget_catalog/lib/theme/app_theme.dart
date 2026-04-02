import 'package:fluent_ui/fluent_ui.dart';

abstract final class AppTheme {
  static FluentThemeData light(Color accent) => FluentThemeData(
    brightness: Brightness.light,
    accentColor: AccentColor.swatch({
      'normal': accent,
      'dark': HSLColor.fromColor(accent).withLightness(0.3).toColor(),
      'darker': HSLColor.fromColor(accent).withLightness(0.2).toColor(),
      'darkest': HSLColor.fromColor(accent).withLightness(0.1).toColor(),
      'light': HSLColor.fromColor(accent).withLightness(0.6).toColor(),
      'lighter': HSLColor.fromColor(accent).withLightness(0.7).toColor(),
      'lightest': HSLColor.fromColor(accent).withLightness(0.8).toColor(),
    }),
    visualDensity: VisualDensity.standard,
  );

  static FluentThemeData dark(Color accent) => FluentThemeData(
    brightness: Brightness.dark,
    accentColor: AccentColor.swatch({
      'normal': accent,
      'dark': HSLColor.fromColor(accent).withLightness(0.3).toColor(),
      'darker': HSLColor.fromColor(accent).withLightness(0.2).toColor(),
      'darkest': HSLColor.fromColor(accent).withLightness(0.1).toColor(),
      'light': HSLColor.fromColor(accent).withLightness(0.6).toColor(),
      'lighter': HSLColor.fromColor(accent).withLightness(0.7).toColor(),
      'lightest': HSLColor.fromColor(accent).withLightness(0.8).toColor(),
    }),
    visualDensity: VisualDensity.standard,
  );
}
