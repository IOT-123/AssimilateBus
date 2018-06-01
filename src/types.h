enum Role {
	SENSOR,
	ACTOR
};

struct NameValue
{
	char name[16];
	char value[16];
};

struct PropViz  // property visualisation definitions
{
	char card_type[16];
	char units[16];
	NameValue icons[2];
	NameValue labels[2];
	NameValue values[3];
	int min;
	int max;
	int total;
	bool is_series;  // whether it gets plotted as a chart
	int high;
	int low;
};

struct NameValuePropViz
{
	byte address;
	char name[16]; // name of property (not sensor)
	char value[16];// val of property
	Role role;
	PropViz prop_vizs;// viz details for 1 property
};
