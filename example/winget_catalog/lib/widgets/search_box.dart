import 'dart:async';

import 'package:fluent_ui/fluent_ui.dart';

/// Debounced search box using Fluent UI AutoSuggestBox style.
class WingetSearchBox extends StatefulWidget {
  final ValueChanged<String> onChanged;

  const WingetSearchBox({super.key, required this.onChanged});

  @override
  State<WingetSearchBox> createState() => _WingetSearchBoxState();
}

class _WingetSearchBoxState extends State<WingetSearchBox> {
  Timer? _debounce;

  @override
  void dispose() {
    _debounce?.cancel();
    super.dispose();
  }

  void _onChanged(String value) {
    _debounce?.cancel();
    _debounce = Timer(const Duration(milliseconds: 400), () {
      widget.onChanged(value.trim());
    });
  }

  @override
  Widget build(BuildContext context) {
    return TextBox(
      placeholder: 'Search packages...',
      prefix: const Padding(
        padding: EdgeInsets.only(left: 8),
        child: Icon(FluentIcons.search, size: 14),
      ),
      onChanged: _onChanged,
    );
  }
}
